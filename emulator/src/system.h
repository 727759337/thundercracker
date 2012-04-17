/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * The whole system- all the hardware we're simulating.
 * This module brings it all together.
 */

#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <string>
#include <glfw.h>
#include "vtime.h"
#include "cube_hardware.h"
#include "system_cubes.h"
#include "system_mc.h"
#include "tracer.h"
#include "tinythread.h"


class System {
 public:
    VirtualTime time;

    static const unsigned DEFAULT_CUBES = 3;
    static const unsigned MAX_CUBES = 32;

    Cube::Hardware cubes[MAX_CUBES];
    Tracer tracer;

    // Static Options; can be set prior to init only
    unsigned opt_numCubes;
    std::string opt_cubeFirmware;

    // Global debug options
    bool opt_continueOnException;
    bool opt_turbo;
    bool opt_lockRotationByDefault;
    bool opt_radioTrace;

    // SVM options
    bool opt_svmTrace;
    bool opt_svmFlashStats;
    std::string opt_elfFile;

    // Debug options, applicable to cube 0 only
    bool opt_cube0Debug;
    std::string opt_cube0Flash;
    std::string opt_cube0Profile;

    System();

    bool init();
    void start();
    void exit();
    void setNumCubes(unsigned n);
    void resetCube(unsigned id);

    bool isRunning() {
        return mIsStarted;
    }

    bool isTraceAllowed();

    // Begin an event that's synchronized with cube execution. Halts the cube thread at 'deadline'.
    void beginCubeEvent(uint64_t deadline) {
        sc.beginEvent(deadline);
    }

    // End an event, resume cube execution.
    void endCubeEvent() {
        sc.endEvent();
    }

 private:
    bool mIsInitialized;
    bool mIsStarted;

    SystemCubes sc;
    SystemMC smc;
};

#endif
