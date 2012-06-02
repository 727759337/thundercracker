/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * LFS layer: our Log-structured File System
 *
 * This is an optional layer which can sit on top of the Volume layer.
 * While it's totally feasible to use Volumes directly, for storing large
 * immutable objects, the LFS is designed to provide an efficient way to
 * store small objects that may change frequently.
 *
 * This LFS is a fairly classic implementation of the general LFS concept.
 * For more background information, check out Wikipedia:
 *
 *   http://en.wikipedia.org/wiki/Log-structured_file_system
 *
 * Each Volume in the filesystem may act as the parent for an LFS, which
 * occupies zero or more child volumes of that parent. These volumes are
 * used as a ring buffer for stored objects.
 *
 * Objects are identified in three ways:
 *
 *   1. Their parentage. Each object is "owned" by a particular volume,
 *      typically a Game or the Launcher. This forms the top-level namespace
 *      under which objects are stored.
 *
 *   2. An 8-bit key identifies distinct objects within a particular parent
 *      volume. These keys are opaque identifiers for us. Games may use
 *      them to differentiate between different kinds of saved data, such
 *      as different types of metrics, or different save-game slots.
 *
 *   3. A version. Because several copies of an object may exist in flash,
 *      we need to know which one is the most recent. This ordering is
 *      implied by the physical order of objects within a single LFS volume,
 *      and the ordering of LFS volumes within a particular LFS is stated
 *      explicitly via a sequence number in the volume header.
 *
 * Each volume has an "index" of objects in that volume, and a "meta-index"
 * which acts as an index for individual blocks of index data.
 *
 * Index blocks grow down from the end of the volume, whereas object data
 * grows up from the beginning of the volume. The maximum number of index
 * blocks is limited both by the amount of space in the volume, and by the
 * number of rows in the (fixed size) meta-index.
 */

#ifndef FLASH_LFS_H_
#define FLASH_LFS_H_

#include "macros.h"
#include "flash_volume.h"
#include "flash_volumeheader.h"


/**
 * Common methods used by multiple LFS components.
 */
namespace LFS {
    uint8_t computeCheckByte(uint8_t a, uint8_t b);
    bool isEmpty(const uint8_t *bytes, unsigned count);
};


/**
 * FlashLFSKeyFilter is a probabilistic data structure that can be used to
 * test whether a key is maybe-in the set or definitely-not-in the set.
 *
 * This is equivalent to a very tiny Bloom filter with a single trivial hash
 * function (k=1). Whereas the generalized Bloom filter is good for cases
 * where the space of keys is very large compared to the size of the filter,
 * this object is intended for cases where there's only an order of magnitude
 * or so of difference: We have 256 possible keys, and 16 bits of hash.
 *
 * These sizes were chosen so that this KeyFilter can be used to build a fast
 * "meta-index" that tells us which index pages we need to examine. This
 * must fit within the type-specific-data region in each Volume.
 *
 * Because this filter must operate in flash memory, and adding an item must
 * not require an erase, we invert the typical Bloom filter bit values. "1"
 * represents an empty hash bucket, "0" means that one or more items occupy
 * that bucket.
 * 
 * We want a hash function with the following properties:
 *
 *   - If a game uses small counting numbers (0-15, for example) it's
 *     guaranteed to get a distinct hash bucket for each key.
 *
 *   - If there are going to be collisions, we'd like a different
 *     set of collisions to happen for each index block. This limits the
 *     extent of the performance damage.
 *
 * To accomplish both of those goals, we use a really simple function:
 *
 *     (key * (1 + 2*row)) % 16
 *
 * This For every odd number M, [(i * M) % 16 for i in range(16)] produces
 * a set which includes all numbers from 0 through 15, with no duplicates.
 */
class FlashLFSKeyFilter
{
    uint16_t bits;

    unsigned bitHash(unsigned row, unsigned key) const {
        // See explanation above.
        unsigned h = (key * ((row << 1) | 1)) & 0xF;

        // We have no need for CLZ here, so use a right-to-left order.
        return 1 << h;
    }

public:
    bool isEmpty() const {
        return bits == 0xFFFF;
    }

    void add(unsigned row, unsigned key) {
        bits &= ~bitHash(row, key);
    }

    /**
     * Returns 'true' if the item is possibly in the filter, or 'false'
     * if it is definitely not.
     */
    bool test(unsigned row, unsigned key) const {
        return !(bits & bitHash(row, key));
    }
};


/**
 * FlashLFSVolumeHeader represents the physical storage format of our
 * volume-type-specific information. This data shares the same FlashBlock
 * as the Volume header (which is guaranteed to be small for LFS volumes).
 *
 * We use this space to store a sequence number for each volume, as well
 * as a "meta-index" of FlashLFSKeyFilters which tell us which index
 * blocks may possibly contain a given record type.
 *
 * Note that this structure is guaranteed to be placed at a 32-bit-aligned
 * address, as all elements in the Volume header format are aligned.
 *
 * Note that volumes can become full either because their payload space has
 * filled up with objects, or because their index is full. The index is
 * almost always large enough that the payload space fills up first, but
 * there is no upper bound on the worst-case index space used. (Due to
 * invalid index records and invalid anchor records)
 */
class FlashLFSVolumeHeader
{
public:
    // Number of meta-index rows that will fit in the available FlashBlock space
    static const unsigned NUM_ROWS =
        (FlashVolumeHeader::MAX_MAPPABLE_DATA_BYTES - 4) / 2;

    uint32_t sequence;
    FlashLFSKeyFilter metaIndex[NUM_ROWS];

    static FlashLFSVolumeHeader *fromVolume(FlashBlockRef &ref, FlashVolume vol);
    bool isRowEmpty(unsigned row) const;
    void add(unsigned row, unsigned key);
    bool test(unsigned row, unsigned key);
    unsigned numNonEmptyRows();
};


/**
 * FlashLFSIndexRecord is the data type which comprises our object index.
 * This stores all of the metadata we need to identify and load an object
 * from the LFS itself.
 *
 * Index records should be small: Larger index records require more flash
 * blocks to store, which means more meta-index records. The meta-index
 * must be small enough to keep in the Volume header block. This is one
 * of the reasons we keep the size of the key space limited. (The other
 * reason is to put a reasonable upper bound on the size of the whole LFS.)
 *
 * This record must hold enough information to (1) locate an object,
 * (2) very reliably determine whether any space has been allocated,
 * even if we experience a power failure during write, and (3) validate
 * the object, so we can fall back on a previous version if it's corrupted.
 *
 * This implementation requires 5 bytes of storage:
 *    - 8-bit key, 8-bit size
 *    - 16-bit CRC for the object contents
 *    - Check byte for key and size
 *
 * This lets us detect the difference between the two main power-fail modes:
 *
 *    1. We started to write the index, but didn't finish. This means
 *       we've wasted an index slot, but no space was actually allocated for
 *       the object, so that space is still free.
 *
 *    2. We finished writing the index, but the object data didn't finish
 *       writing. This will render the content CRC invalid, so we'll
 *       ignore this and use an earlier version of the same object.
 */
class FlashLFSIndexRecord
{
    uint8_t key;
    uint8_t size;
    uint8_t crc[2];     // Unaligned on purpose...
    uint8_t check;      //   to keep the check byte at the end.

public:
    // Keys are 8-bit, there can be at most this many
    static const unsigned MAX_KEYS = 0x100;

    // Object sizes are 8-bit, nonzero, measured in multiples of SIZE_UNIT
    static const unsigned SIZE_SHIFT = 4;
    static const unsigned SIZE_UNIT = 1 << SIZE_SHIFT;
    static const unsigned SIZE_MASK = SIZE_UNIT - 1;
    static const unsigned MAX_SIZE = 0x100 << SIZE_SHIFT;
    static const unsigned MIN_SIZE = 0x001 << SIZE_SHIFT;

    void init(unsigned key, unsigned sizeInBytes, unsigned crc)
    {
        STATIC_ASSERT(sizeof *this == 5);

        ASSERT(key < MAX_KEYS);
        ASSERT(sizeInBytes >= MIN_SIZE);
        ASSERT(sizeInBytes <= MAX_SIZE);
        ASSERT((sizeInBytes & SIZE_MASK) == 0);

        unsigned size = (sizeInBytes >> SIZE_SHIFT) - 1;

        this->key = key;
        this->size = size;
        this->crc[0] = crc;
        this->crc[1] = crc >> 8;
        this->check = LFS::computeCheckByte(key, size);
    }

    bool isValid() const {
        return check == LFS::computeCheckByte(key, size);
    }

    bool isEmpty() const {
        return LFS::isEmpty((const uint8_t*) this, sizeof *this);
    }

    unsigned getKey() const {
        return key;
    }

    unsigned getSizeInBytes() const {
        return (size + 1U) << SIZE_SHIFT;
    }

    bool checkCRC(unsigned reference) const {
        return !(((crc[0] | (crc[1] << 8)) ^ reference) & 0xFFFF);
    }

    static bool isKeyAllowed(unsigned key) {
        return key < MAX_KEYS;
    }

    static bool isSizeAllowed(unsigned bytes) {
        return bytes <= MAX_SIZE;
    }
};


/**
 * FlashLFSIndexAnchor is a flavor of header record that appears at
 * the beginning of an index block. Typically there is exactly
 * one anchor per block, but we must allow for the possibility of multiple
 * anchors, in case there's a power failure while allocating a new index
 * block. Anchors must be validated, and invalid anchors are ignored.
 * There must be exactly one valid anchor per block. Records begin
 * immediately after the first valid anchor.
 *
 * Anchors are used to locate the object data which corresponds with the
 * first record in any given index block. The anchor value could also
 * be determined by summing the sizes of all preceeding index records, but
 * including this value explicitly in the anchor lets us locate the objects
 * associated with an index block without reading any additional index blocks.
 *
 * An offset of zero is valid. Erased (0xFF) memory is guaranteed to never
 * be interpreted as a valid FlashLFSIndexAnchor.
 */
class FlashLFSIndexAnchor
{
    uint8_t offset[2];
    uint8_t check;

public:
    static const unsigned OFFSET_SHIFT = FlashLFSIndexRecord::SIZE_SHIFT;
    static const unsigned OFFSET_UNIT = FlashLFSIndexRecord::SIZE_UNIT;
    static const unsigned OFFSET_MASK = FlashLFSIndexRecord::SIZE_MASK;
    static const unsigned MAX_OFFSET = FlashMapBlock::BLOCK_SIZE;

    void init(unsigned offsetInBytes)
    {
        STATIC_ASSERT(sizeof offset == 2);
        STATIC_ASSERT(sizeof *this == 3);
        STATIC_ASSERT((MAX_OFFSET >> OFFSET_SHIFT) < 0xFFFF);

        ASSERT((offsetInBytes & OFFSET_MASK) == 0);
        ASSERT(offsetInBytes < MAX_OFFSET);

        unsigned offsetWord = offsetInBytes >> OFFSET_SHIFT;
        uint8_t offsetLow = offsetWord;
        uint8_t offsetHigh = offsetWord >> 8;

        this->offset[0] = offsetLow;
        this->offset[1] = offsetHigh;
        this->check = LFS::computeCheckByte(offsetLow, offsetHigh);
    }

    bool isValid() const {
        return check == LFS::computeCheckByte(offset[0], offset[1]);
    }

    bool isEmpty() const {
        return LFS::isEmpty((const uint8_t*) this, sizeof *this);
    }

    unsigned getOffsetInBytes() const {
        return (this->offset[0] | (this->offset[1] << 8)) << OFFSET_SHIFT;
    }
};


// Out-of-class for accessing records and anchors (Avoids circular dependencies)
namespace LFS {
    
    // Return the first possible location for a record in a block
    inline FlashLFSIndexRecord *firstRecord(FlashLFSIndexAnchor *anchor) {
        ASSERT(anchor);
        return (FlashLFSIndexRecord *) (anchor + 1);
    }

    // Return the last possible location for an anchor in a block
    inline FlashLFSIndexRecord *lastRecord(FlashBlock *block) {
        uint8_t *p = block->getData() + FlashBlock::BLOCK_SIZE;
        return ((FlashLFSIndexRecord *) p) - 1;
    }

    // Return the first possible location for an anchor in a block
    inline FlashLFSIndexAnchor *firstAnchor(FlashBlock *block) {
        return (FlashLFSIndexAnchor *) block->getData();
    }

    // Return the last possible location for an anchor in a block
    inline FlashLFSIndexAnchor *lastAnchor(FlashBlock *block) {
        uint8_t *p = block->getData() + FlashBlock::BLOCK_SIZE;
        return ((FlashLFSIndexAnchor *) p) - 1;
    }
}


/**
 * FlashLFSIndexBlockIter is an iterator that knows how to interpret the
 * anchors and records present in a single index block.
 *
 * Index blocks contain, in order:
 *
 *   1. Zero or more invalid anchors
 *   2. Exactly one valid anchor
 *   3. Zero or more records
 *   4. Zero or more unprogrammed (0xFF) bytes
 *
 * The anchor specifies a base address for the indexed objects, whereas
 * the records specify each object's type and size. The Index block alone
 * is enough information to compute the address of any object in the block.
 */
class FlashLFSIndexBlockIter
{
    FlashBlockRef blockRef;
    FlashLFSIndexAnchor *anchor;
    FlashLFSIndexRecord *currentRecord;
    unsigned currentOffset;

public:
    FlashLFSIndexBlockIter(uint32_t blockAddr) {
        beginBlock(blockAddr);
    }

    bool beginBlock(uint32_t blockAddr);
    bool prev();
    bool next();

    const FlashLFSIndexRecord* operator->() const {
        ASSERT(currentRecord);
        return currentRecord;
    }

    unsigned getCurrentOffset() const {
        return currentOffset;
    }
};


/**
 * FlashLFSVolumeVector is an ordered list of Volumes which comprise a
 * single logical filesystem. The volumes in a VolumeVector are always
 * sorted by sequence number. Slots in the vector may be marked as invalid
 * due to deletion, and they must be skipped until the vector is compacted.
 *
 * The vector must be sized to support a worst-case filesystem layout. This
 * means the maximum number of keys, and every object taking the maximum
 * amount of space. Plus we need room for two partially-filled volumes at
 * the beginning and end of our ring.
 */
class FlashLFSVolumeVector
{
    // Maximum size of our index, per volume
    static const unsigned MAX_INDEX_BYTES_PER_VOLUME =
        FlashBlock::BLOCK_SIZE * FlashLFSVolumeHeader::NUM_ROWS;

    // Minimum amount of volume space available for object data
    static const unsigned MIN_OBJ_BYTES_PER_VOLUME =
        FlashMapBlock::BLOCK_SIZE   // Our volumes are a single map block long
        - FlashBlock::BLOCK_SIZE    // Volume header takes one cache block
        - MAX_INDEX_BYTES_PER_VOLUME;

    // Theoretical maximum amount of space used by all objects
    static const unsigned MAX_TOTAL_OBJ_BYTES =
        FlashLFSIndexRecord::MAX_KEYS * FlashLFSIndexRecord::MAX_SIZE;

    // Number of volumes required just to store the worst-case set of objects
    static const unsigned MAX_OBJ_VOLUMES = 
        (MAX_TOTAL_OBJ_BYTES + MIN_OBJ_BYTES_PER_VOLUME - 1) / MIN_OBJ_BYTES_PER_VOLUME;

    // Number of 'padding' volumes which we may require for garbage collection
    static const unsigned PAD_VOLUMES = 2;

public:
    // Maximum number of volumes in use by a single filesystem
    static const unsigned MAX_VOLUMES = MAX_OBJ_VOLUMES + PAD_VOLUMES;

    FlashVolume slots[MAX_VOLUMES];
    unsigned numSlotsInUse;
};


/**
 * Represents the in-memory state associated with a single LFS.
 *
 * This is a vector of FlashVolumes for each volume within the LFS.
 * Each of those volumes have been sorted in order of ascending sequence
 * number.
 */
class FlashLFS
{
private:
    FlashLFSVolumeVector volumes;

public:
    FlashLFS(FlashVolume parent);

    bool findObject(unsigned key, uint32_t &addr, uint32_t &size);
        // Scan meta-index backwards, for candidate blocks
        // Scan blocks forward, to establish anchor
        // Scan backwards until we match
        // Return address and size of object (guaranteed linear)
    
    bool newObject(unsigned key, uint32_t size, uint32_t crc, uint32_t &addr);
        // Jump to last block in meta-index. If it's full, allocate a new block and write an anchor
        // Write an index record (allocates space for the object)
        // Return location at which object data can be written.
        // Write the object data (may be split among multiple flash blocks)

    bool collectGarbage();
        // Scan backwards, keeping a bitmap of all keys we've found
        // Any volume consisting of only superceded keys is deleted
        
        // Also: If the oldest volume is less than X amount full, copy
        //       all non-superceded keys, then delete it.
        //       XXX - how to do this for more than one volume at a time?
};

#endif
