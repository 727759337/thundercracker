/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _MOTION_H
#define _MOTION_H

#include <sifteo/abi.h>
#include <protocol.h>
#include "macros.h"


/**
 * Static utilities for dealing with motion buffers
 */

class MotionUtil {
public:

    /// Convert a raw ACK buffer to a packed _SYSByte4 accelerometer reading.
    static _SYSByte4 captureAccelState(const RF_ACKType &ack);

private:
    MotionUtil();    // Do not implement
};


/**
 * MotionWriter knows how to enqueue raw accelerometer samples
 * into a userspace _SYSMotionBuffer. This is used by CubeSlot to
 * buffer accelerometer data received in its radio ACK callback.
 */

class MotionWriter {
public:
    _SYSMotionBuffer *mbuf;

};


#endif
