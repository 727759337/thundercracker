/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

/*
 * Common to all boards
 */
namespace Board {
    static const uint8_t* UniqueId = (uint8_t*)0x1FFFF7E8;
    static const unsigned UniqueIdNumBytes = 12;
}

// available boards to choose from
#define BOARD_TC_MASTER_REV1    1
#define BOARD_TC_MASTER_REV2    2
#define BOARD_TEST_JIG          3

// default board
#ifndef BOARD
#define BOARD   BOARD_TC_MASTER_REV2
#endif

#include "hardware.h"

#if BOARD == BOARD_TC_MASTER_REV2
#include "board/rev2.h"
#elif BOARD == BOARD_TC_MASTER_REV1
#include "board/rev1.h"
#elif BOARD == BOARD_TEST_JIG
#include "board/testjig.h"
#else
#error BOARD not configured
#endif

#endif // BOARD_H
