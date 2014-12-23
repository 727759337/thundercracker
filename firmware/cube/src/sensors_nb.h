/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
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
 
/*
 * Internal declarations for neighbor sensors
 */

#ifndef _SENSORS_NB_H
#define _SENSORS_NB_H

/*
 * Parameters that affect the neighboring protocol. Some of these are
 * baked in to some extent, and not easily changeable with
 * #defines. But they're documented here anyway.
 *
 *  Timer period:    162.76 Hz   Overflow rate of 13-bit Timer 0 at clk/12
 *  Packet length:   16 bits     Long enough for one data byte and one check byte
 *  # of timeslots:  24          Maximum number of supported cubes in master
 *  Timeslot size:   256 us      Period is split evenly into 24 slots
 *
 * These parameters on their own would give us a maximum bit period of
 * 16 us.  But that assumes that we can perfectly synchronize all
 * cubes. The more margin we can give ourselves for timer drift and
 * communication latency, the better. Our electronics seem to be able
 * to handle pulses down to ~4 us, but it's also better not to push these
 * limits.
 *
 * The other constraint on bit rate is that we need it to divide evenly
 * into our clk/12 timebase (0.75 us).
 *
 * To ensure that we stay within our timeslot even if we can't begin
 * transmitting immediately, we have a deadline, measured in Timer 0
 * ticks (clk/12). The maximum value of this deadline is our timeslot
 * size minus the packet length. But we'll make it a little bit
 * shorter still, so that we preserve our margin for radio latency.
 *
 * Our current packet format is based on a 16-bit frame, so that we can
 * quickly store it using a 16-bit shift register. Currently, for packet
 * validation, we simply define the second 8 bits to be the complement
 * of the first 8 bits. So, we really have only an 8-bit frame. From
 * MSB to LSB, it is defined as:
 *
 *    [7  ] -- Always one. In the first byte, this serves as a start bit,
 *             and the receiver does not store it explicitly. In the second
 *             byte, it just serves as an additional check.
 *
 *    [6:5] -- Mask bits. Transmitted as ones, but by applying different
 *             side masks for each bit, we can discern which side the packet
 *             was received from.
 *
 *    [4:0] -- ID for the transmitting cube.
 */

#define NBR_TX      // comment out to turn off neighbor TX
#define NBR_RX      // comment out to turn off neighbor RX

#ifdef NBR_RX
#define NBR_SQUELCH_ENABLE          // Enable squelching during neighbor RX
#endif

#define NB_TX_BITS          16      // 1 header, 2 mask, 13 payload
#define NB_RX_BITS          16      // 1 header, 2 mask, 13 payload

// 2us pulses, 15.75us bit periods, 252us packets
#define NB_BIT_TICKS        21      // In 0.75 us ticks
#define NB_BIT_TICK_FIRST   14      // Sampling tweak to ignore secondary pulses
#define NB_DEADLINE         22      // Max amount Timer0 can be late by

extern uint8_t nb_bits_remaining;   // Bit counter for transmit or receive
extern uint8_t nb_buffer[2];        // Packet shift register for TX/RX

extern __bit nb_tx_mode;            // We're in the middle of an active transmission
extern __bit nb_rx_mask_state0;
extern __bit nb_rx_mask_state1;
extern __bit nb_rx_mask_state2;
extern __bit nb_rx_mask_bit0;
extern __bit nb_rx_mask_bit1;

/*
 * Our current neighbor ID. Valid IDs have the three MSBs set to 1,
 * i.e. they're already formatted properly for use as the first byte
 * of the raw neighbor packet.
 *
 * If this is zero (the default while disconnected), neighbor transmit
 * is disabled.
 *
 * Note: On real hardware, this is overlaid with an unused SFR purely
 *       to save RAM. But in Siftulator, this also gives us an easy
 *       way to peek at the cube's assigned neighbor ID and show
 *       that ID in the heads-up display.
 */
__sfr __at 0xA1 nb_tx_id;

/*
 * We do a little bit of signal conditioning on neighbors before
 * passing them on to the ACK packet. There are really two things we
 * need to do:
 *
 *   1. Detect when a neighbor has disappeared
 *   2. Smooth over momentary glitches
 *
 * For (1), we use the master sensor clock as a sort of watchdog for
 * the neighbors. If our communication were always perfect, we would
 * get one packet per active neighbor per master sensor period. So,
 * we'll start with that as a baseline. That leads into part (2)... if
 * any state change occurs (neighboring or de-neighboring) we need it
 * to stay stable for at least a couple consecutive frames before we
 * report that over the radio.
 */

// Neighbor state for the current Timer 0 period
extern uint8_t __idata nb_instant_state[4];

// State that we'd like to promote to the ACK packet, if we can verify it.
extern uint8_t __idata nb_prev_state[4];

/*
 * Shared neighbor state transition code
 */

#pragma sdcc_hash +

// Prepare for TF1 interrupt on received start bit, let LC tanks float
#define NB_BEGIN_RX()                           __endasm; \
    __asm mov     TL1, #0xFF                    __endasm; \
    __asm mov     TH1, #0xFF                    __endasm; \
    __asm clr     _nb_tx_mode                   __endasm; \
    __asm setb    _TCON_TR1                     __endasm; \
    __asm orl     _MISC_DIR, #MISC_NB_OUT       __endasm; \
    __asm

// Go to transmit mode, set up bit period timer
#define NB_BEGIN_TX()                               __endasm; \
    __asm setb    _nb_tx_mode                       __endasm; \
    __asm mov     _nb_bits_remaining, #NB_TX_BITS   __endasm; \
    __asm mov     _TL2, #(0x100 - NB_BIT_TICKS)     __endasm; \
    __asm setb    _T2CON_T2I0                       __endasm; \
    __asm

#endif
