/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * swiss - your Sifteo utility knife
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lfsvolume.h"

#include <map>
#include <errno.h>

using namespace std;


uint8_t SwissLFS::computeCheckByte(uint8_t a, uint8_t b)
{
    /*
     * Generate a check-byte value which is dependent on all bits
     * in 'a' and 'b', but is guaranteed not to be 0xFF.
     * (So we can differentiate this from an unprogrammed byte.)
     *
     * Any single-bit error in either 'a' or 'b' is guaranteed
     * to produce a different check byte value, even if we truncate
     * the most significant bit.
     *
     * This pattern of bit rotations means that it's always legal
     * to chop off the MSB. We only actually do this, though, if
     * the result otherwise would have been 0xFF.
     *
     * XOR'ing in a constant ensures that a run of zeroes isn't
     * misinterpreted as valid data.
     */

    uint8_t result = 0x42 ^ a ^ ROR8(b, 1) ^ ROR8(a, 3) ^ ROR8(b, 5);

    if (result == 0xFF)
        result = 0x7F;

    return result;
}

bool SwissLFS::isEmpty(const uint8_t *bytes, unsigned count)
{
    // Are the specified bytes all unprogrammed?

    while (count) {
        if (bytes[0] != 0xFF)
            return false;
        bytes++;
        count--;
    }
    return true;
}


LFSVolume::LFSVolume(unsigned cacheBlockSize, unsigned blockSize) :
    CacheBlockSize(cacheBlockSize),
    BlockSize(blockSize),
    objectDataIndex(0)
{
}

bool LFSVolume::init(FILE *f)
{
    long pos = ftell(f);

    if (fread(&header, sizeof header, 1, f) != 1) {
        fprintf(stderr, "couldn't init from file\n");
        return false;
    }

    if (header.type != FlashVolume::T_LFS) {
        fprintf(stderr, "not an LFS volume!\n");
        return false;
    }

    if (!header.isHeaderValid()) {
        return false;
    }

    // payload is located CacheBlockSize after the beginning of the header
    if (fseek(f, pos + CacheBlockSize, SEEK_SET) != 0) {
        fprintf(stderr, "couldn't seek to payload: %ld %s\n", pos, strerror(errno));
        return false;
    }

    uint8_t buf[BlockSize - CacheBlockSize];
    int rxed = fread(buf, 1, sizeof buf, f);
    if (rxed <= 0) {
        return false;
    }
    payload.insert(payload.begin(), buf, buf + rxed);

    return true;
}

void LFSVolume::retrieveRecords(SaveData::Records & records)
{
    /*
     * Index blocks descend from the end of the payload, while
     * LFS object data ascends from the beginning of the payload.
     */

    int start = payload.size() - CacheBlockSize;

    unsigned indexRecordCount = 0;

    while (start > 0) {
        vector<uint8_t> block(payload.begin() + start,
                              payload.begin() + start + CacheBlockSize);
        bool done = retrieveRecordsFromBlock(records, block);
        indexRecordCount++;
        if (done) {
            return;
        }

        start -= CacheBlockSize;
    }
}

bool LFSVolume::retrieveRecordsFromBlock(SaveData::Records &records, vector<uint8_t> &indexBlock)
{
    /*
     * Return true if we're at the end of valid data.
     */

    unsigned idx = 0;
    bool foundAnchor = false;

    /*
     * First find the anchor
     * there may be invalid anchors before we find a good one
     */

    while (idx < indexBlock.size() - 3) {
        FlashLFSIndexAnchor anchor(&indexBlock[idx]);
        idx += 3;

        if (anchor.isValid()) {
            foundAnchor = true;
            break;
        }
    }

    if (!foundAnchor) {
        return false;
    }

    /*
     * we're now at the beginning of the index records
     * keep going until we run into unprogrammed territory
     */

    while (idx < indexBlock.size() - 5) {
        FlashLFSIndexRecord rec(&indexBlock[idx]);
        idx += 5;

        if (rec.isEmpty()) {
            return true;
        }

        if (rec.isValid()) {
            // XXX: recalculate CRC to be sure/paranoid
            unsigned objSize = rec.sizeInBytes();

            vector<uint8_t> objectData(payload.begin() + objectDataIndex,
                                       payload.begin() + objectDataIndex + objSize);

            SaveData::Record r(rec.key, rec.crc16(), objSize, objectData);
            records[rec.key].push_back(r);

            objectDataIndex += objSize;
        }
    }

    return idx < indexBlock.size() - 5;
}
