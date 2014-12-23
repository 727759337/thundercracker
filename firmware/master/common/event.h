/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker firmware
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

#ifndef _SIFTEO_RUNTIME_H
#define _SIFTEO_RUNTIME_H

#include <sifteo/abi.h>
#include "machine.h"
#include "svm.h"
#include "svmruntime.h"
#include "bits.h"
using namespace Svm;


/**
 * Event dispatcher
 *
 * Pending events are tracked with a hierarchy of change bitmaps.
 * Bitmaps should always be set from most specific to least specific.
 * For example, with asset downloading, an individual cube's bit
 * in assetDoneCubes.
 *
 * Since the change bits also implicitly prioritize events,
 * and we want to be able to rearrange the priority of events
 * without breaking ABI, we introduce one layer of indirection. This
 * class defines its own Priority IDs (PID) which are used in these
 * change bitmaps.
 *
 * Events with lower priority ID numbers are higher priority, and
 * vice versa.
 */

class Event {
 public:

    // This must stay in sync with pidToVector[] in event.cpp!
    enum PriorityID {

        // Audio events (latency sensitive)
        PID_BASE_TRACKER,

        // High priority: Cube disconnect/connect events
        PID_CONNECTION,

        // Screen refresh. Must be lower priority than connection
        PID_CUBE_REFRESH,

        // System state events (lower than refresh)
        PID_BASE_GAME_MENU,
        PID_CUBE_ASSETDONE,

        // Filesystem events (delete before commit)
        PID_BASE_VOLUME_DELETE,
        PID_BASE_VOLUME_COMMIT,

        // USB events (disconnect before connect)
        PID_BASE_USB_DISCONNECT,
        PID_BASE_USB_CONNECT,
        PID_BASE_USB_READ_AVAILABLE,
        PID_BASE_USB_WRITE_AVAILABLE,

        // Bluetooth events (disconnect before connect)
        PID_BASE_BT_DISCONNECT,
        PID_BASE_BT_CONNECT,
        PID_BASE_BT_READ_AVAILABLE,
        PID_BASE_BT_WRITE_AVAILABLE,

        // Low-bandwidth sensor events
        PID_NEIGHBORS,
        PID_CUBE_TOUCH,

        // High-bandwidth / low-priority events
        PID_CUBE_BATTERY,
        PID_CUBE_ACCELCHANGE,

        // Must be last
        NUM_PIDS
    };

    // Only called by SvmRuntime. Do not call directly from syscalls!
    static void dispatch();

    static void clearVectors();

    static void setBasePending(PriorityID pid, uint32_t param=0);
    static void setCubePending(PriorityID pid, _SYSCubeID cid);

    static void setVector(_SYSVectorID vid, void *handler, void *context);
    static void *getVectorHandler(_SYSVectorID vid);
    static void *getVectorContext(_SYSVectorID vid);

    static bool callNeighborEvent(_SYSVectorID vid, _SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1);

 private:
    struct VectorInfo {
        reg_t handler;
        reg_t context;
    };

    union Params {
        _SYSCubeIDVector cubesPending;      /// CLZ map of pending cubes
        uint32_t generic;
    };

    static bool callBaseEvent(_SYSVectorID vid, uint32_t param);
    static bool callCubeEvent(_SYSVectorID vid, _SYSCubeID cid);

    static bool dispatchCubePID(PriorityID pid, _SYSCubeID cid);
    static bool dispatchBasePID(PriorityID pid, _SYSVectorID vid);
    static void cubeEventsClear(PriorityID pid);

    static VectorInfo vectors[_SYS_NUM_VECTORS];
    static Params params[NUM_PIDS];
    static BitVector<NUM_PIDS> pending;
};


#endif
