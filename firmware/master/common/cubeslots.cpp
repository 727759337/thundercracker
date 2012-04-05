/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "cubeslots.h"
#include "cube.h"
#include "neighbors.h"
#include "machine.h"


CubeSlot CubeSlots::instances[_SYS_NUM_CUBE_SLOTS];

_SYSCubeIDVector CubeSlots::vecEnabled = 0;
_SYSCubeIDVector CubeSlots::vecConnected = 0;
_SYSCubeIDVector CubeSlots::flashResetWait = 0;
_SYSCubeIDVector CubeSlots::flashResetSent = 0;
_SYSCubeIDVector CubeSlots::flashACKValid = 0;
_SYSCubeIDVector CubeSlots::frameACKValid = 0;
_SYSCubeIDVector CubeSlots::neighborACKValid = 0;
_SYSCubeIDVector CubeSlots::expectStaleACK = 0;
_SYSCubeIDVector CubeSlots::flashAddrPending = 0;
_SYSCubeIDVector CubeSlots::hwidValid = 0;

_SYSCubeID CubeSlots::minCubes = 0;
_SYSCubeID CubeSlots::maxCubes = _SYS_NUM_CUBE_SLOTS;

_SYSAssetLoader *CubeSlots::assetLoader = 0;


void CubeSlots::solicitCubes(_SYSCubeID min, _SYSCubeID max)
{
	minCubes = min;
	maxCubes = max;
}

void CubeSlots::enableCubes(_SYSCubeIDVector cv)
{
    NeighborSlot::resetSlots(cv);
    Atomic::Or(CubeSlots::vecEnabled, cv);
}

void CubeSlots::disableCubes(_SYSCubeIDVector cv)
{
    Atomic::And(CubeSlots::vecEnabled, ~cv);
}

void CubeSlots::connectCubes(_SYSCubeIDVector cv)
{
    Atomic::Or(CubeSlots::vecConnected, cv);

    // Expect that the cube's radio may have one old ACK packet buffered. Ignore this packet.
    Atomic::Or(CubeSlots::expectStaleACK, cv);
}

void CubeSlots::disconnectCubes(_SYSCubeIDVector cv)
{
    Atomic::And(CubeSlots::vecConnected, ~cv);

    Atomic::And(CubeSlots::flashResetWait, ~cv);
    Atomic::And(CubeSlots::flashResetSent, ~cv);
    Atomic::And(CubeSlots::flashACKValid, ~cv);
    Atomic::And(CubeSlots::neighborACKValid, ~cv);
    Atomic::And(CubeSlots::hwidValid, ~cv);
    NeighborSlot::resetSlots(cv);
    NeighborSlot::resetPairs(cv);

    // TODO: if any of the cubes in cv are currently part of a
    // neighbor-pair with any cubes that are still active, those
    // active cubes neeed to remove their now-defunct neighbors
}

void CubeSlots::paintCubes(_SYSCubeIDVector cv)
{
    /*
     * If a previous repaint is still in progress, wait for it to
     * finish. Then trigger a repaint on all cubes that need one.
     *
     * Since we always send VRAM data to the radio in order of
     * increasing address, having the repaint trigger (vram.flags) at
     * the end of memory guarantees that the remainder of VRAM will
     * have already been sent by the time the cube gets the trigger.
     *
     * Why does this operate on a cube vector? Because we want to
     * trigger all cubes at close to the same time. So, we first wait
     * for all cubes to finish their last paint, then we trigger all
     * cubes.
     */

    _SYSCubeIDVector waitVec = cv;
    while (waitVec) {
        _SYSCubeID id = Intrinsic::CLZ(waitVec);
        CubeSlots::instances[id].waitForPaint();
        waitVec ^= Intrinsic::LZ(id);
    }

    SysTime::Ticks timestamp = SysTime::ticks();

    _SYSCubeIDVector paintVec = cv;
    while (paintVec) {
        _SYSCubeID id = Intrinsic::CLZ(paintVec);
        CubeSlots::instances[id].triggerPaint(timestamp);
        paintVec ^= Intrinsic::LZ(id);
    }
}

void CubeSlots::finishCubes(_SYSCubeIDVector cv)
{
    while (cv) {
        _SYSCubeID id = Intrinsic::CLZ(cv);
        CubeSlots::instances[id].waitForFinish();
        cv ^= Intrinsic::LZ(id);
    }
}
