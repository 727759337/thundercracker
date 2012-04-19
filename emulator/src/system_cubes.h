/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _SYSTEM_CUBES_H
#define _SYSTEM_CUBES_H

#include "tinythread.h"
#include "macros.h"
#include "deadlinesynchronizer.h"

class System;


class SystemCubes {
 public:
    bool init(System *sys);
    void exit();

    void start();
    void stop();

    void setNumCubes(unsigned n);
    void resetCube(unsigned id);

    // Allow other threads to synchronize with cube execution
    DeadlineSynchronizer deadlineSync;

 private: 
    void startThread();
    void stopThread();

    static void threadFn(void *param);
    bool initCube(unsigned id, bool wakeFromSleep=false);
    void exitCube(unsigned id);

    ALWAYS_INLINE void tick(unsigned count=1);
    NEVER_INLINE void tickLoopDebug();
    NEVER_INLINE void tickLoopGeneral();
    NEVER_INLINE void tickLoopFastSBT();

    System *sys;
    tthread::thread *mThread;
    bool mThreadRunning;
};

#endif
