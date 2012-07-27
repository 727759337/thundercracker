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
#include <sifteo/abi.h>

#include "vtime.h"
#include "cube_hardware.h"
#include "system_cubes.h"
#include "system_mc.h"
#include "tracer.h"
#include "tinythread.h"
#include "flash_storage.h"


class System {
 public:
    VirtualTime time;

    static const unsigned DEFAULT_CUBES = 3;
    static const unsigned MAX_CUBES = _SYS_NUM_CUBE_SLOTS;

    Cube::Hardware cubes[MAX_CUBES];
    Tracer tracer;
    FlashStorage flash;

    // Static Options; can be set prior to init only
    bool opt_headless;
    unsigned opt_numCubes;
    std::string opt_cubeFirmware;
    std::string opt_flashFilename;
    std::string opt_launcherFilename;
    std::string opt_waveoutFilename;

    // UI options
    bool opt_whiteBackground;
    int opt_windowWidth;
    int opt_windowHeight;

    // Global debug options
    bool opt_continueOnException;
    bool opt_turbo;
    bool opt_lockRotationByDefault;
    bool opt_radioTrace;
    bool opt_traceEnabledAtStartup;
    bool opt_noCubeReconnect;

    // Master firmware debug options
    bool opt_paintTrace;

    // SVM options
    bool opt_svmTrace;
    bool opt_svmFlashStats;
    bool opt_svmStackMonitor;
    unsigned opt_gdbServerPort;

    // Debug options, applicable to cube 0 only
    bool opt_cube0Debug;
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

    DeadlineSynchronizer &getCubeSync() {
        return sc.deadlineSync;
    }

    void stopCubesOnly() {
        sc.stop();
    }

 private:
    bool mIsInitialized;
    bool mIsStarted;

    SystemCubes sc;
    SystemMC smc;
};

#endif
