/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _NEIGHBOR_PROTOCOL_H
#define _NEIGHBOR_PROTOCOL_H

#include <stdint.h>


namespace Neighbor {

    /*
     * Units are in APB1 ticks, which has a rate of 36MHz.
     */
    static const unsigned TICK_HZ = 36000000;

    /*
     * Timers are set to the neighbor transmission's bit width of 16us.
     * Pulse duration is 2us.
     *
     * 2us  / (1 / 36000000) == 72 ticks
     * 12us / (1 / 36000000) == 432 ticks (this is the old bit period width, if you ever need it)
     * 16us / (1 / 36000000) == 576 ticks
     */
    static const unsigned PULSE_LEN_TICKS = 72;
    static const unsigned BIT_PERIOD_TICKS = 576;

    // Number of bits to wait for during an RX sequence
    static const unsigned NUM_RX_BITS = 16;


    /*
     * Number of bit periods to wait between transmissions.
     *
     * We need at least one successful transmit per 162 Hz sensor polling interval,
     * and we need to speak with unpaired cubes which won't be synchronized with our
     * clock yet. So, currently we target a transmit rate of about twice that. If we
     * ever miss two polling intervals in a row, the cube will perceive that we've
     * de-neighbored.
     */
    static const unsigned NUM_TX_WAIT_PERIODS = 192;

    /*
     * Neighbor ID constants
     */

    static const unsigned FIRST_CUBE_ID = 0xE0;
    static const unsigned NUM_CUBE_ID = 24;
    static const unsigned FIRST_MASTER_ID = FIRST_CUBE_ID + NUM_CUBE_ID;
    static const unsigned NUM_MASTER_ID = 8;
}

#endif
