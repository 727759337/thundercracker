/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _UI_COORDINATOR_H
#define _UI_COORDINATOR_H

#include <sifteo/abi.h>
#include "systime.h"
#include "ui_assets.h"


/**
 * This object handles high-level cube management and state management
 * for the built-in UI screens. It tracks connected and disconnected
 * cubes, manages stippling and restoring cubes, and handles video buffer
 * and paint state.
 *
 * This is used by all of the built-in UIs except for UIPanic, which
 * takes a much more minimal approach in order to run in any execution
 * context.
 */

class UICoordinator {
public:
    UICoordinator(uint32_t excludedTasks=0);

    // Poll for new cubes
    _SYSCubeIDVector connectCubes();

    // Bulk operations on all cubes
    void stippleCubes(_SYSCubeIDVector cv);
    void restoreCubes(_SYSCubeIDVector cv);

    // One-cube-at-a-time operations, using 'avb'
    bool pollForAttach();
    void attachToCube(_SYSCubeID id);
    void paint();
    void finish();
    void detach();
    bool isTouching();

    void idle();

    bool ALWAYS_INLINE isAttached() {
        return _SYSCubeID(avb.cube) != _SYSCubeID(_SYS_CUBE_ID_INVALID);
    }

    uint32_t excludedTasks;
    _SYSCubeIDVector uiConnected;
    UIAssets assets;
    _SYSAttachedVideoBuffer avb;

    /*
     * Drawing utilities
     */

    ALWAYS_INLINE unsigned xy(unsigned x, unsigned y) {
        return x + y * _SYS_VRAM_BG0_WIDTH;
    }

    void setPanX(int x);
    void setPanY(int y);
    void setMode(unsigned mode = _SYS_VM_BG0_ROM);
    void letterboxWindow(unsigned height);
    void drawTiles(unsigned dest, const uint16_t *src, unsigned count, unsigned palette = 0);

private:
    _SYSVideoBuffer *savedVBuf;
    SysTime::Ticks stippleDeadline;
};


#endif
