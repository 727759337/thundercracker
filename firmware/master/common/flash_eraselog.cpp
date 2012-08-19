/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "flash_volume.h"
#include "flash_recycler.h"
#include "flash_eraselog.h"
#include "crc.h"

FlashEraseLog::FlashEraseLog()
    : volume(FlashMapBlock::invalid()), readIndex(0), writeIndex(0)
{}


unsigned FlashEraseLog::indexToFlashAddress(unsigned index)
{
    ASSERT(index < NUM_RECORDS);
    return volume.block.address() + FlashBlock::BLOCK_SIZE + sizeof(Record) * index;
}

unsigned FlashEraseLog::readFlag(unsigned index)
{
    uint8_t flag;
    FlashDevice::read(indexToFlashAddress(index) + offsetof(Record, flag), &flag, sizeof flag);
    return flag;
}

void FlashEraseLog::writePopFlag(unsigned index)
{
    uint8_t flag = F_POPPED;
    FlashDevice::write(indexToFlashAddress(index) + offsetof(Record, flag), &flag, sizeof flag);
}    

void FlashEraseLog::readRecord(Record &r, unsigned index)
{
    FlashDevice::read(indexToFlashAddress(index), (uint8_t*) &r, sizeof r);
}

void FlashEraseLog::writeRecord(Record &r, unsigned index)
{
    FlashDevice::write(indexToFlashAddress(index), (uint8_t*) &r, sizeof r);
}

void FlashEraseLog::findIndices()
{
    /*
     * Prepare to start using a new volume; from 'volume', figure out the
     * initial values of readIndex and writeIndex by binary search.
     */

    // Half-open interval
    unsigned begin = 0;
    unsigned end = NUM_RECORDS;

    // Binary search for the last F_POPPED record.
    while (begin + 1 < end) {
        unsigned middle = (begin + end) >> 1;
        ASSERT(middle < end);
        if (readFlag(middle) == F_POPPED) {
            // After or equal to this record
            begin = middle;
        } else {
            // Before this record
            end = middle;
        }
    }

    // Either we found no F_POPPED records at all, or we found the last one.
    // Note that if *all* records have been popped, readIndex == NUM_RECORDS.
    ASSERT(end == 0 || begin + 1 == end);

    // Start reading just after the last F_POPPED record
    readIndex = end;
    ASSERT(readIndex == NUM_RECORDS || readFlag(readIndex) != F_POPPED);
    ASSERT(readIndex == 0 || readFlag(readIndex - 1) == F_POPPED);

    // Binary search for the last non-erased record
    end = NUM_RECORDS;
    while (begin + 1 < end) {
        unsigned middle = (begin + end) >> 1;
        ASSERT(middle < end);
        if (readFlag(middle) == F_ERASED) {
            // Before this record
            end = middle;
        } else {
            // After or equal to this record
            begin = middle;
        }
    }

    // Start writing at the first erased record. If the volume is full,
    // writeIndex == NUM_RECORDS.
    writeIndex = end;
    ASSERT(writeIndex == NUM_RECORDS || readFlag(writeIndex) == F_ERASED);
    ASSERT(writeIndex == 0 || readFlag(writeIndex - 1) != F_ERASED);
}

bool FlashEraseLog::allocate()
{
    /*
     * Make sure we have space for one more record: We must have a valid
     * volume, and writeIndex < NUM_RECORDS. Returns 'true' on success, or
     * 'false' if we're out of space.
     */

    FlashVolumeIter vi;
    FlashVolume v;
    vi.begin();

    while (!volume.block.isValid() || writeIndex >= NUM_RECORDS) {

        // Look for a volume of the right type
        do {
            if (!vi.next(v)) {
                // Out of volumes to search. Try allocating a new volume.

                FlashVolumeWriter vw;
                if (!vw.begin(FlashVolume::T_ERASE_LOG, NUM_RECORDS * sizeof(Record), 0, FlashMapBlock::invalid()))
                    return false;
                vw.commit();

                volume = vw.volume;
                readIndex = 0;
                writeIndex = 0;
                return true;
            }
        } while (v.getType() != FlashVolume::T_ERASE_LOG);

        volume = v;
        findIndices();
    }

    // Appending to an existing log volume
    return true;
}

uint16_t FlashEraseLog::computeCheck(const Record &r)
{
    Crc32::reset();
    Crc32::add(r.ec);
    Crc32::add(r.block.code);
    return Crc32::get();
}

void FlashEraseLog::commit(Record &rec)
{
    /*
     * Space has already been allocated. Just write this record to flash.
     */

    ASSERT(writeIndex < NUM_RECORDS);
    ASSERT(volume.block.isValid());

    rec.flag = F_VALID;
    rec.check = computeCheck(rec);

    writeRecord(rec, writeIndex++);
}

bool FlashEraseLog::pop(Record &rec)
{
    /*
     * Dequeue the oldest item from the FlashEraseLog. This searches for
     * a log volume if necessary. Any fully-used volumes are deleted.
     *
     * Returns true and populates 'rec' if we find a record. This record
     * must be used in a new volume, or the block will be orphaned.
     */

    FlashVolumeIter vi;
    FlashVolume v;
    vi.begin();

    // Loop until we get a record with a valid CRC
    while (1) {

        // Find a good volume
        while (!volume.block.isValid() || readIndex >= NUM_RECORDS) {

            // Look for a volume of the right type
            do {
                if (!vi.next(v))
                    return false;
            } while (v.getType() != FlashVolume::T_ERASE_LOG);

            volume = v;
            findIndices();

            if (readIndex >= NUM_RECORDS)
                volume.deleteSingle();
        }

        readRecord(rec, readIndex);
        ASSERT(rec.flag == F_ERASED || rec.flag == F_VALID);

        // End of iteration?
        if (rec.flag == F_ERASED)
            return false;

        // Consume this record
        if (rec.flag != F_POPPED)
            writePopFlag(readIndex);
        readIndex++;

        // Skip bad records, only return good ones
        if (rec.flag == F_VALID && computeCheck(rec) == rec.check)
            return true;
    }
}

// Tell our FlashBlockRecycler not to use the erase log
FlashBlockPreEraser::FlashBlockPreEraser()
    : recycler(false)
{}

bool FlashBlockPreEraser::next()
{
    /*
     * Recycle and log one more block.
     *
     * Ask the Recycler to bypass using the erase log, otherwise
     * we would not be guaranteed to make forward progress here.
     */

    if (!log.allocate())
        return false;

    FlashEraseLog::Record r;
    if (!recycler.next(r.block, r.ec))
        return false;

    log.commit(r);
    return true;
}
