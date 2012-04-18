/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _CUBE_H
#define _CUBE_H

#include <sifteo/abi.h>
#include "machine.h"
#include "radio.h"
#include "svmmemory.h"
#include "cubeslots.h"
#include "systime.h"
#include "cubecodec.h"
#include "paintcontrol.h"


/**
 * One cube, or potential cube. The firmware is built with a fixed
 * number of statically allocated cube slots. Under application
 * control, these slots can be enabled. When a slot is enabled, we try
 * to fill it with a physical cube. Once a physical cube connects, we
 * can start writing to it.
 *
 * So, a slot only needs to track some very basic cube state. Slots
 * are intended to be very lightweight, with any larger buffers
 * allocated and attached to the slot by application code.
 */

class CubeSlot {
 public:
    bool radioProduce(PacketTransmission &tx);
    void radioAcknowledge(const PacketBuffer &packet);
    void radioTimeout();

    _SYSCubeID id() const {
        _SYSCubeID i = this - &CubeSlots::instances[0];
        ASSERT(i < _SYS_NUM_CUBE_SLOTS);
        STATIC_ASSERT(arraysize(CubeSlots::instances) == _SYS_NUM_CUBE_SLOTS);
        return i;
    }

    static CubeSlot &getInstance(_SYSCubeID id) {
        ASSERT(id < _SYS_NUM_CUBE_SLOTS);
        return CubeSlots::instances[id];
    }

    _SYSCubeIDVector bit() const {
        STATIC_ASSERT(_SYS_NUM_CUBE_SLOTS <= 32);
        return Intrinsic::LZ(id());
    }

    bool enabled() const {
        return !!(bit() & CubeSlots::vecEnabled);
    }
    
    bool connected() const {
        return !!(bit() & CubeSlots::vecConnected);
    }
    
    void setConnected() {
        CubeSlots::connectCubes(Intrinsic::LZ(id()));
    }
    
    void setDisconnected() {
        CubeSlots::disconnectCubes(Intrinsic::LZ(id()));
    }

    void setVideoBuffer(_SYSVideoBuffer *v) {
        vbuf = v;
    }

    const _SYSByte4 &getAccelState() {
        return accelState;
    }

    inline const uint8_t *getRawNeighbors() const {
        return neighbors;
    }

    bool isTouching() const;

    _SYSAssetGroupCube *assetGroupCube(struct _SYSAssetGroup *group) {
        _SYSAssetGroupCube *cube = reinterpret_cast<_SYSAssetGroupCube*>(group + 1) + id();
        if (SvmMemory::mapRAM(cube, sizeof *cube))
            return cube;
        else
            return 0;
    }

    _SYSAssetLoaderCube *assetLoaderCube(struct _SYSAssetLoader *loader) {
        _SYSAssetLoaderCube *cube = reinterpret_cast<_SYSAssetLoaderCube*>(loader + 1) + id();
        if (SvmMemory::mapRAM(cube, sizeof *cube))
            return cube;
        else
            return 0;
    }

    bool isAssetLoading(_SYSAssetLoader *L) const {
        // Caller-provided SYSAssetLoader, to support cases when we
        // must read "assetLoader" exactly once.
        return L && (L->cubeVec & ~L->complete & bit());
    }

    bool isAssetLoading() const {
        return isAssetLoading(CubeSlots::assetLoader);
    }

    void startAssetLoad(SvmMemory::VirtAddr groupVA, uint16_t baseAddr);

    void waitForPaint() {
        paintControl.waitForPaint(this);
    }

    void waitForFinish() {
        // Finish is only meaningful when we still have a vbuf attached.
        if (vbuf)
            paintControl.waitForFinish(this);
    }

    void triggerPaint(SysTime::Ticks timestamp) {
        // Allow continuous rendering only when not loading assets
        paintControl.triggerPaint(this, timestamp);
    }

    uint64_t getHWID();

    uint16_t getRawBatteryV() const {
        return rawBatteryV;
    }

    uint8_t getLastFrameACK() const {
        return framePrevACK;
    }

    bool hasValidFrameACK() const {
        return CubeSlots::frameACKValid & bit();
    }

    _SYSVideoBuffer *getVBuf() const {
        return vbuf;
    }

    const RadioAddress *getRadioAddress();

 private:
    // Limit on round-trip time
    static const unsigned RTT_DEADLINE_MS = 250;

    /*
     * Data buffers, provided by game code.
     *
     * 'vbuf' is non-NULL any time this cube has a buffer attached. We
     * will try to send out any changes in that buffer. The buffer
     * pointer is guaranteed to remain valid until it's set to NULL or
     * to a different pointer, which can happen only outside of IRQ
     * context.
     */

    _SYSVideoBuffer *vbuf;
    RadioAddress address;
    
    DEBUG_ONLY(SysTime::Ticks assetLoadTimestamp);
    
    SysTime::Ticks flashDeadline;       // Used only by ISR
    uint32_t timeSyncState;             // XXX: For the current time-sync hack

    PaintControl paintControl;
    CubeCodec codec;

    // Byte variables
    uint8_t flashPrevACK;
    uint8_t framePrevACK;
    uint8_t neighbors[4];
    uint8_t hwid[_SYS_HWID_BYTES];

    // Other sensor data
    _SYSByte4 accelState;
    uint16_t rawBatteryV;
    
    void requestFlashReset();
    uint16_t calculateTimeSync();
};

#endif
