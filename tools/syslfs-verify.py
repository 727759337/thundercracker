#!/usr/bin/env python

#
# Tool to verify SysLFS log contents in a filesystem dump.
# Can operate on persistent filesystem files from the Siftulator (run with -F)
# or the output from `swiss backup` (capturing filesystem contents from hardware).
#

import sys, struct, array, StringIO
from SiftulatorFlash import *

# SysLFS info: firmware/master/common/flash_syslfs.h

_SYS_ASSET_SLOTS_PER_BANK   = 4

ASSET_SLOTS_PER_CUBE    = _SYS_ASSET_SLOTS_PER_BANK * 2
NUM_PAIRINGS            = 24 # _SYS_NUM_CUBE_SLOTS
NUM_TOTAL_ASSET_SLOTS   = NUM_PAIRINGS * ASSET_SLOTS_PER_CUBE
NUM_FAULTS              = 16

# Keys
kFaultBase      = 0x16
kPairingMRU     = kFaultBase + NUM_FAULTS
kPairingID      = kPairingMRU + 1
kCubeBase       = kPairingID + 1
kAssetSlotBase  = kCubeBase + NUM_PAIRINGS
kEnd            = kAssetSlotBase + NUM_TOTAL_ASSET_SLOTS

####################################################
# find all SysLFS blocks - type T_LFS and no parent
####################################################

def findSysLFSBlocks():

    blocks = []

    for i in range(storageFile.mcNumBlocks()):

        vol = storageFile.mcVolume(i + 1)
        if vol.isValid() and vol.type == T_LFS and vol.parentBlock == 0:
            blocks.append(vol.blockCode)

    return blocks


def keyDetail(key):
    """
    Return a string suitable for printing that describes the given key
    """

    if key < kFaultBase:
        return "0x%x - reserved key  " % key

    if key < kPairingMRU:
        return "0x%x - fault key     " % key

    if key == kPairingMRU:
        return "0x%x - PairingMRU key" % key

    if key == kPairingID:
        return "0x%x - PairingID key " % key

    if key < kAssetSlotBase:
        return "0x%x - Cube key      " % key

    if key < kEnd:
        return "0x%x - AssetSlot key " % key

    raise ValueError("unhandled key value")

def verifyIndexBlock(block):
    """
    For a given index block, traverse its contents and look for invalid records.
    TODO: validate CRCs on object data.
    """

    valid = 0
    invalid = 0

    anchor = None
    # first find a valid index
    while block.tell() < (block.len - 3):
        anchor = FlashLFSIndexAnchor(block.read(3))
        if anchor.isValid():
            break

    # did we find a good one?
    if not anchor.isValid():
        return True, valid, invalid

    # iterate index records
    while block.tell() < (block.len - 5):
        record = FlashLFSIndexRecord(block.read(5))
        # empty records indicate the end of the block
        if record.isEmpty():
            return True, valid, invalid

        # print "%s sizeInBytes %d" % (keyDetail(record.key), record.sizeInBytes())
        if not record.isValid():
            invalid = invalid + 1
            # print "!!! invalid record"
        else:
            valid = valid + 1

    return False, valid, invalid

def verifySysLFSIndex(blockCode):
    """
    Step through the index block and verify its records are valid
    """

    indexBlockLen = 0x100
    vol = storageFile.mcVolume(blockCode)

    end = len(vol.payload)
    start = end - indexBlockLen

    indexBlocks = 0
    validRecords = 0
    invalidRecords = 0

    while start > 0:
        block = StringIO.StringIO(vol.payload[start:end])
        done, valid, invalid = verifyIndexBlock(block)
        indexBlocks = indexBlocks + 1
        if done is True:
            break

        validRecords = validRecords + valid
        invalidRecords = invalidRecords + invalid

        start = start - indexBlockLen
        end = end - indexBlockLen

    print "verified SysLFS block %d (index %d).\n\tindexBlocks traversed: %d\n\tvalidRecords: %d\n\tinvalidRecords %d" % \
            (blockCode, blockCode - 1, indexBlocks, validRecords, invalidRecords)


####################################################
# main
####################################################

path = sys.argv[1]
storageFile = StorageFile(path)

syslfsBlocks = findSysLFSBlocks()
if len(syslfsBlocks) == 0:
    print "no SysLFS blocks in this FS, sorry"
    sys.exit(0)

for block in syslfsBlocks:
    verifySysLFSIndex(block)

