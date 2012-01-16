/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _CUBE_NEIGHBORS_H
#define _CUBE_NEIGHBORS_H

#include <stdint.h>

#include "tracer.h"
#include "cube_cpu.h"
#include "cube_cpu_reg.h"
#include "vtime.h"

namespace Cube {

class Hardware;


/*
 * Our neighbor sensors can transmit or receive small inductive
 * pulses.  In the new hardware design for Thundercracker, all
 * receivers share a single amplifier. So, the signal we receive is
 * more like a logical OR of the pulses coming in from each side. This
 * signal drives an IRQ GPIO.
 *
 * We have one bidirectional GPIO pin per side which is used for dual
 * purposes. Edge transitions on these pins will resonate in the
 * transmitter's LC tank, sending a pulse to any nearby receiver. The
 * pins can also be used to squelch a single receiver channel. If the
 * pin is floating, the channel participates in the OR above. If the
 * pin is driven, the input is masked.
 *
 * XXX: This hardware simulation is very basic. Right now we just
 *      propagate pulses from transmitter to receiver in a totally
 *      idealized fashion. There are all sorts of analog electronic
 *      effects in this circuit which can make it sensitive to changes
 *      in the firmware. Especially pulse width (must resonate with
 *      the LC tank) and pulse spacing, and the possibility of
 *      detecting false/reflected pulses.
 */

class Neighbors {
 public:
    enum Side {
        TOP = 0,
        LEFT,
        BOTTOM,
        RIGHT,
        NUM_SIDES,
    };

    void init() {
        memset(&mySides, 0, sizeof mySides);
    };
    
    void attachCubes(Hardware *cubes);

    void setContact(unsigned mySide, unsigned otherSide, unsigned otherCube) {
        /* Mark two neighbor sensors as in-range. ONLY called by the UI thread. */
        mySides[mySide].otherSides[otherSide] |= 1 << otherCube;
    }

    void clearContact(unsigned mySide, unsigned otherSide, unsigned otherCube) {
        /* Mark two neighbor sensors as not-in-range. ONLY called by the UI thread. */
        mySides[mySide].otherSides[otherSide] &= ~(1 << otherCube);
    }
    
    static ALWAYS_INLINE void clearNeighborInput(CPU::em8051 &cpu) {
        // Immediately after we handle timer edges, auto-clear the neighbor input
        if (cpu.mSFR[PORT] & PIN_IN) {
            cpu.mSFR[PORT] &= ~PIN_IN;
            cpu.needTimerEdgeCheck = true;
        }
    }
    
    static void receivedPulse(CPU::em8051 &cpu) {
        Tracer::log(&cpu, "NEIGHBOR: Received pulse");

        cpu.mSFR[PORT] |= PIN_IN;
        cpu.needTimerEdgeCheck = true;
    }

    void ioTick(CPU::em8051 &cpu);

 private:
    void transmitPulse(CPU::em8051 &cpu, unsigned otherCube, uint8_t otherSide);

    static const unsigned PORT          = REG_P1;
    static const unsigned DIR           = REG_P1DIR;
    static const unsigned PIN_IN        = (1 << 6);
    static const unsigned PIN_0_TOP     = (1 << 0);
    static const unsigned PIN_1_LEFT    = (1 << 1);
    static const unsigned PIN_2_BOTTOM  = (1 << 7);
    static const unsigned PIN_3_RIGHT   = (1 << 5);
    
    uint8_t inputMask;
    uint8_t prevDriveHigh;

    struct {
        uint32_t otherSides[NUM_SIDES];
    } mySides[NUM_SIDES];

    Hardware *otherCubes;
};


};  // namespace Cube

#endif
