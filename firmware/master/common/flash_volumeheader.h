/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Part of the flash Volume layer: This file defines the layout of
 * the header on each volume. Each header contains:
 *
 *   - Prefix: Magic number, CRCs, type code, flags
 *   - Optional FlashMap, identifying all MapBlocks in the volume
 *   - Optional erase counts for all MapBlocks in the volume
 *
 * A volume header is responsible for storing this information on behalf of
 * the entire volume. For volumes consisting of multiple blocks, subsequent
 * blocks have no header of their own. This keeps the MapSpans for the volume
 * contiguous.
 *
 * Because the volume header is responsible for storing erase counts, and
 * we must take care to preserve these erase counts even in unused areas of
 * the flash, we need a way to mark the volume as "deleted" without
 * invalidating the other data stored in this header.
 */

#ifndef FLASH_VOLUMEHEADER_H_
#define FLASH_VOLUMEHEADER_H_

#include "macros.h"
#include "flash_map.h"
#include "crc.h"


struct FlashVolumeHeader
{
    /*
     * Fixed-size portion of the header (32 bytes)
     */

    uint64_t magic;                 // Must equal MAGIC

    uint16_t type;
    uint16_t payloadBlocks;         // Number of FlashBlocks of payload data
    uint16_t dataBytes;             // Type-specific data size
    uint16_t payloadBlocksCpl;      // Redundant complement of numBlocks
    uint16_t dataBytesCpl;          // Redundant complement of dataSize
    uint16_t typeCopy;              // Redundant copy of 'type'

    uint32_t crcMap;                // CRC of in-use portion of FlashMap
    uint32_t crcErase;              // CRC of in-use portion of erase count map

    uint8_t  parentBlock;           // FlashMapBlock code for parent block (0 if top-level)
    uint8_t  parentBlockCpl;        // Redundant complement of parentBlock

    uint16_t reserved;              // Must be 0xFFFF

    /*
     * Followed by variable-sized parts:
     *
     *   - FlashMap (guaranteed to fit wholly in one cache block)
     *     (Padding to the next 32-bit boundary)
     *   - Optional type-specific data
     *     (Padding to the next 32-bit boundary)
     *   - Erase count map
     *     (Padding to the next cache block boundary)
     *   - Payload data
     */

    static const uint64_t MAGIC = 0x5f4c4f5674666953ULL;
    typedef uint32_t EraseCount;

    /**
     * Maximum size of type-specific data that will fit within the same
     * FlashBlock as the FlashVolumeHeader, given a minimal single-block
     * volume header.
     */
    static const unsigned MAX_MAPPABLE_DATA_BYTES =
        FlashBlock::BLOCK_SIZE - 32 - 4 - 4;

    static FlashVolumeHeader *get(FlashBlockRef &ref)
    {
        return reinterpret_cast<FlashVolumeHeader*>(ref->getData());
    }

    static FlashVolumeHeader *get(FlashBlockRef &ref, FlashMapBlock mb)
    {
        FlashBlock::get(ref, mb.address());
        return get(ref);
    }

    /// Check whether the FlashVolumeHeader itself is superficially valid.
    bool isHeaderValid() const
    {
        STATIC_ASSERT(sizeof payloadBlocks == 2);
        STATIC_ASSERT(sizeof dataBytes == 2);
        STATIC_ASSERT(sizeof parentBlock == 1);

        return magic == MAGIC &&
               type == typeCopy &&
               (payloadBlocks ^ payloadBlocksCpl) == 0xFFFF &&
               (dataBytes ^ dataBytesCpl) == 0xFFFF &&
               (parentBlock ^ parentBlockCpl) == 0xFF;
    }

    /// Initializes the FlashVolumeHeader, except for CRC fields
    void init(unsigned type, unsigned payloadBlocks, unsigned dataBytes,
        FlashMapBlock parent)
    {
        STATIC_ASSERT(sizeof this->parentBlock == sizeof parent.code);

        this->magic = MAGIC;
        this->type = type;
        this->payloadBlocks = payloadBlocks;
        this->dataBytes = dataBytes;
        this->payloadBlocksCpl = ~payloadBlocks;
        this->dataBytesCpl = ~dataBytes;
        this->typeCopy = type;
        this->parentBlock = parent.code;
        this->parentBlockCpl = ~parent.code;
        this->reserved = 0xFFFF;

        ASSERT(isHeaderValid());
    }

    /// Initializes only the 'type' fields
    void setType(unsigned type)
    {
        this->type = type;
        this->typeCopy = type;
        ASSERT(isHeaderValid());
    }

    /// Payload size, in bytes
    unsigned payloadBytes() const
    {
        return payloadBlocks * FlashBlock::BLOCK_SIZE;
    }

    /// Number of map entries, equal to the number of FlashMapBlocks we cover
    unsigned numMapEntries() const
    {
        /*
         * This is actually kind of tricky to calculate!
         *
         * Since the exact size of the header depends on the total size of
         * header plus payload, that means that we can easily end up with a
         * circular dependency here.
         *
         * We can't use the worst-case size of the map here, since that will
         * cause us to waste ~600 bytes of space in the worst case. It's
         * especially noticeable on LFS blocks, where we size the payload
         * to exactly fit the volume.
         *
         * We also can't solve this as a system of linear equations, because
         * rounding up for alignment makes this nonlinear.
         *
         * On the bright side, it isn't possible for the header to add more
         * than a single map block to the size of the volume. So, calculate
         * it once assuming a minimum header size, then if that turns out
         * to be too small for the actual header we'd need, add one.
         */

        unsigned minResult = ceildiv<unsigned>(
            payloadBlocks + 1,
            FlashMapBlock::BLOCK_SIZE / FlashBlock::BLOCK_SIZE);

        unsigned minHdrBlocks = ceildiv<unsigned>(
            sizeof(*this) +                                     // Fixed header
            roundup<4>(sizeof(FlashMapBlock) * minResult) +     // Map
            roundup<4>(dataBytes) +                             // Type-specific data
            sizeof(EraseCount) * minResult,                     // Erase counts
            FlashBlock::BLOCK_SIZE);

        if (ceildiv<unsigned>(payloadBlocks + minHdrBlocks,
            FlashMapBlock::BLOCK_SIZE / FlashBlock::BLOCK_SIZE) == minResult)
            return minResult;
        else
            return minResult + 1;
    }

    /// Offset to the beginning of the FlashMap, in bytes
    static unsigned mapOffsetBytes()
    {
        return sizeof(FlashVolumeHeader);
    }

    /// Size of the in-use portion of our FlashMap, in bytes
    static unsigned mapSizeBytes(unsigned numMapEntries)
    {
        return roundup<4>(sizeof(FlashMapBlock) * numMapEntries);
    }

    /// Offset to the type-specific data, in bytes
    static unsigned dataOffsetBytes(unsigned numMapEntries)
    {
        return mapOffsetBytes() + mapSizeBytes(numMapEntries);
    }

    /// Offset to the erase counts, in bytes
    static unsigned eraseCountOffsetBytes(unsigned numMapEntries, unsigned dataBytes)
    {
        return dataOffsetBytes(numMapEntries) + roundup<4>(dataBytes);
    }

    /// Offset to the payload data, in bytes
    static unsigned payloadOffsetBytes(unsigned numMapEntries, unsigned dataBytes)
    {
        return eraseCountOffsetBytes(numMapEntries, dataBytes) + (sizeof(EraseCount) * numMapEntries);
    }

    /// Offset to the payload data, in cache blocks
    static unsigned payloadOffsetBlocks(unsigned numMapEntries, unsigned dataBytes)
    {
        return (payloadOffsetBytes(numMapEntries, dataBytes) + FlashBlock::BLOCK_MASK) / FlashBlock::BLOCK_SIZE;
    }

    /// Retrieve a pointer to the FlashMap, always part of the same cache block.
    FlashMap *getMap()
    {
        STATIC_ASSERT(sizeof(FlashMap) + sizeof(*this) <= FlashBlock::BLOCK_SIZE);
        return reinterpret_cast<FlashMap*>(this + 1);
    }

    /// Const version of getMap()
    const FlashMap *getMap() const
    {
        return reinterpret_cast<const FlashMap*>(this + 1);
    }

    /// Calculate the CRC of our FlashMap
    uint32_t calculateMapCRC(unsigned numMapEntries) const
    {
        return Crc32::block(reinterpret_cast<const uint32_t*>(getMap()),
            mapSizeBytes(numMapEntries) / sizeof(uint32_t));
    }

    /// Retrieve the absolute address of a single erase count
    static unsigned eraseCountAddress(FlashMapBlock mb, unsigned index, unsigned numMapEntries, unsigned dataBytes)
    {
        ASSERT(index < numMapEntries);
        return mb.address() + eraseCountOffsetBytes(numMapEntries, dataBytes) + index * sizeof(EraseCount);
    }

    /// Retrieve an erase count value
    EraseCount getEraseCount(FlashBlockRef &ref, FlashMapBlock mb, unsigned index, unsigned numMapEntries) const
    {
        unsigned addr = eraseCountAddress(mb, index, numMapEntries, dataBytes);
        unsigned blockPart = addr & ~FlashBlock::BLOCK_MASK;
        unsigned bytePart = addr & FlashBlock::BLOCK_MASK;
        ASSERT((bytePart % sizeof(EraseCount)) == 0);

        FlashBlock::get(ref, blockPart);
        return *reinterpret_cast<EraseCount*>(ref->getData() + bytePart);
    }

    /// Calculate the CRC of our erase count array
    uint32_t calculateEraseCountCRC(FlashMapBlock mb, unsigned numMapEntries) const
    {
        FlashBlockRef ref;
        Crc32::reset();
        for (unsigned I = 0; I != numMapEntries; ++I)
            Crc32::add(getEraseCount(ref, mb, I, numMapEntries));
        return Crc32::get();
    }
};


#endif
