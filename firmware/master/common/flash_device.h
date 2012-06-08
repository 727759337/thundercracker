/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * The lowest (first) layer of the flash stack: Physical access to the
 * flash device. The implementation of this layer is platform-specific.
 */

#ifndef FLASH_DEVICE_H
#define FLASH_DEVICE_H

#include <stdint.h>
#include "macros.h"

class FlashDevice {
public:
    static const unsigned PAGE_SIZE = 256;              // programming granularity
    static const unsigned SECTOR_SIZE = 1024 * 4;       // smallest erase granularity
    static const unsigned CAPACITY = 1024 * 1024 * 16;  // total storage capacity

    static const uint8_t MACRONIX_MFGR_ID = 0xC2;

    static void init();

    static void read(uint32_t address, uint8_t *buf, unsigned len);
    static void write(uint32_t address, const uint8_t *buf, unsigned len);
    static bool writeInProgress();

    DEBUG_ONLY(static void verify(uint32_t address, const uint8_t *buf, unsigned len);)

    static void eraseSector(uint32_t address);
    static void chipErase();

    struct JedecID {
        uint8_t manufacturerID;
        uint8_t memoryType;
        uint8_t memoryDensity;
    };

    static void readId(JedecID *id);
};

#endif

