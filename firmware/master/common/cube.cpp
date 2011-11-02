/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <protocol.h>
#include <sifteo/machine.h>

#include "cube.h"
#include "vram.h"

using namespace Sifteo;

/*
 * Frame rate control parameters
 *
 * fpsLow --
 *    "Minimum" frame rate. If we're waiting more than this long
 *    for a frame to render, give up. Prevents us from getting wedged
 *    if a cube stops responding.
 *
 * fpsHigh --
 *    Maximum frame rate. Paint will always block until at least this
 *    long since the previous frame, in order to provide a global rate
 *    limit for the whole app.
 *
 * fpContinuous --
 *    When at least this many frames are pending acknowledgment, we
 *    switch to continuous rendering mode. Instead of waiting for a
 *    signal from us, the cubes will just render continuously.
 *
 * fpSingle --
 *    The inverse of fpContinuous. When at least this few frames are
 *    pending, we switch back to one-shot triggered rendering.
 *
 * fpMax --
 *    Maximum number of pending frames to track. If we hit this limit,
 *    Paint() calls will block.
 *
 * fpMin --
 *    Minimum number of pending frames to track. If we go below this
 *    limit, we'll start ignoring acknowledgments.
 */

static const SysTime::Ticks fpsLow = SysTime::hzTicks(4);
static const SysTime::Ticks fpsHigh = SysTime::hzTicks(60);
static const int8_t fpContinuous = 3;
static const int8_t fpSingle = -4;
static const int8_t fpMax = 5;
static const int8_t fpMin = -8;

/*
 * Slot instances
 */

CubeSlot CubeSlot::instances[_SYS_NUM_CUBE_SLOTS];
_SYSCubeIDVector CubeSlot::vecEnabled;
_SYSCubeIDVector CubeSlot::flashResetWait;
_SYSCubeIDVector CubeSlot::flashResetSent;
_SYSCubeIDVector CubeSlot::flashACKValid;
_SYSCubeIDVector CubeSlot::frameACKValid;
_SYSCubeIDVector CubeSlot::neighborACKValid;


void CubeSlot::loadAssets(_SYSAssetGroup *a) {
    _SYSAssetGroupCube *ac = assetCube(a);

    if (!ac)
        return;
    if (isAssetGroupLoaded(a))
        return;

    const _SYSAssetGroupHeader *hdr = a->hdr;
    if (!Runtime::checkUserPointer(hdr, sizeof *hdr))
        return;
    
    // XXX: Pick a base address too!
    ac->progress = 0;

    LOG(("FLASH[%d]: Sending asset group %p, %d bytes\n", id(), a, hdr->dataSize));

    DEBUG_ONLY({
        // In debug builds, we log the asset download time
        assetLoadTimestamp = SysTime::ticks();
    });

    // Start by resetting the flash decoder. This must happen before we set 'loadGroup'.
    Atomic::And(flashResetSent, ~bit());
    Atomic::Or(flashResetWait, bit());

    // Then start streaming asset data for this group
    a->reqCubes |= bit();
    loadGroup = a;
}

bool CubeSlot::radioProduce(PacketTransmission &tx)
{
    /*
     * XXX: Pairing. Try to connect, if we aren't connected. And use a real address.
     *      For now I'm hardcoding the default address, since that's what
     *      the emulator will come up with.
     */

    address.channel = 0x02;
    address.id[0] = id();
    address.id[1] = 0xe7;
    address.id[2] = 0xe7;
    address.id[3] = 0xe7;
    address.id[4] = 0xe7;

    tx.dest = &address;
    tx.packet.len = 0;

    // First priority: Send video buffer updates

    codec.encodeVRAM(tx.packet, vbuf);

    // Second priority: Download assets to flash

    if (flashResetWait & bit()) {
        /*
         * We need to reset the flash decoder before we can send any data.
         *
         * We can only do this if a reset is needed, hasn't already
         * been sent. Send the reset token, and synchronously reset
         * any flash-related IRQ state.
         *
         * Note the flash reset's dual purpose, of both resetting the
         * cube's flash state machine and triggering the cube to send
         * us an ACK packet with a valid flash byte count. So, we
         * actually end up sending two resets if we haven't yet seen a
         * valid flash ACK from this cube.
         */

        if (flashResetSent & bit()) {
            // Already sent the reset. Has it timed out?

            if (SysTime::ticks() > flashDeadline) {
                DEBUG_LOG(("FLASH[%d]: Reset timeout\n", id()));
                Atomic::ClearLZ(flashResetSent, id());
            }

        } else if (codec.flashReset(tx.packet)) {
            // Okay, we sent a reset. Remember to wait for the ACK.

            DEBUG_LOG(("FLASH[%d]: Sending reset token\n", id()));
            Atomic::SetLZ(flashResetSent, id());
            flashDeadline = SysTime::ticks() + SysTime::msTicks(RTT_DEADLINE_MS);
        }

    } else {
        // Not waiting on a reset. See if we need to send asset data.

        bool done = false;
        _SYSAssetGroup *group = loadGroup;

        if (group && !(group->doneCubes & bit()) &&
            codec.flashSend(tx.packet, group, assetCube(group), done)) {

            if (done) {
                /* Finished asset loading */
                Atomic::SetLZ(group->doneCubes, id());
                Atomic::SetLZ(Event::assetDoneCubes, id());
                Event::setPending(Event::ASSET_DONE);

                DEBUG_ONLY({
                    // In debug builds only, we log the asset download time
                    float seconds = (SysTime::ticks() - assetLoadTimestamp) * (1.0f / SysTime::sTicks(1));
                    LOG(("FLASH[%d]: Finished loading group %p in %.3f seconds\n",
                         id(), group, seconds));
                })
            }
        }
    }

    /*
     * Third priority: Sensor time synchronization
     *
     * XXX: Time syncs are kind of special.  We use them to assign
     *      each cube to a different timeslice of our sensor polling
     *      period, allowing the neighbor sensors to cooperate via
     *      time division multiplexing. The packet itself is a short
     *      (3 byte) and simple packet which simply adjusts the phase
     *      of the cube's sensor timer.
     *
     *      We'll need to do some work on the master to calculate this
     *      phase by using the current time plus an estimate of radio
     *      latency. We'll also want to disable hardware retries on
     *      these packets, so that we always have the best possible
     *      control over our latency.
     *
     *      This is totally fake for now. We just rely on getting
     *      lucky, and having few enough cubes that it's likely we do.
     */

    if (timeSyncState)  {
        timeSyncState--;
    } else if (tx.packet.len == 0) {
        timeSyncState = 10000;
        codec.timeSync(tx.packet, id() << 12);
        return true;
    }

    // Finalize this packet. Must be last.
    codec.endPacket(tx.packet);

    /*
     * XXX: We don't have to always return true... we can return false if
     *      we have no useful work to do, so long as we still occasionally
     *      return true to request a ping packet at some particular interval.
     */
    return true;
}

void CubeSlot::radioAcknowledge(const PacketBuffer &packet)
{
    RF_ACKType *ack = (RF_ACKType *) packet.bytes;

    if (packet.len >= offsetof(RF_ACKType, frame_count) + sizeof ack->frame_count) {
        // This ACK includes a valid frame_count counter

        if (frameACKValid & bit()) {
            // Some frame(s) finished rendering.

            uint8_t frameACK = ack->frame_count - framePrevACK;
            Atomic::Add(pendingFrames, -(int32_t)frameACK);

        } else {
            Atomic::SetLZ(frameACKValid, id());
        }

        framePrevACK = ack->frame_count;
    }

    if (packet.len >= offsetof(RF_ACKType, flash_fifo_bytes) + sizeof ack->flash_fifo_bytes) {
        // This ACK includes a valid flash_fifo_bytes counter

        if (flashACKValid & bit()) {
            // Two valid ACKs in a row, we can count bytes.

            uint8_t loadACK = ack->flash_fifo_bytes - flashPrevACK;

            if (flashResetWait & bit()) {
                // We're waiting on a reset
                if (loadACK)
                    Atomic::ClearLZ(flashResetWait, id());
            } else {
                // Acknowledge FIFO bytes

                /*
                 * XXX: Since we can always lose ACK packets without
                 *      warning, we could theoretically deadlock here,
                 *      where we perpetually wait on an ACK that the
                 *      cube has already sent. Since flash writes are
                 *      somewhat pipelined, in practice we'd actually
                 *      have to lose several ACKs in a row. But it
                 *      could indeed happen. We should have a watchdog
                 *      here, to assume FIFO has been drained (or
                 *      explicitly request another flash ACK) if we've
                 *      been waiting for more than some safe amount of
                 *      time.
                 *
                 *      Alternatively, we could solve this on the cube
                 *      end by having it send a longer-than-strictly-
                 *      necessary ACK packet every so often.
                 */

                codec.flashAckBytes(loadACK);
            }

        } else {
            // Now we've seen one ACK. If we're doing a reset, send the token again.
           
            Atomic::SetLZ(flashACKValid, id());
            Atomic::ClearLZ(flashResetSent, id());            
        }

        flashPrevACK = ack->flash_fifo_bytes;
    }

    if (packet.len >= offsetof(RF_ACKType, accel) + sizeof ack->accel) {
        // Has valid accelerometer data. Is it different from our previous state?

        int8_t x = ack->accel[0];
        int8_t y = ack->accel[1];

        if (x != accelState.x || y != accelState.y) {
            accelState.x = x;
            accelState.y = y;
            Atomic::SetLZ(Event::accelChangeCubes, id());
            Event::setPending(Event::ACCEL_CHANGE);
        }
    }

    if (packet.len >= offsetof(RF_ACKType, neighbors) + sizeof ack->neighbors) {
        // Has valid neighbor/flag data

        if (neighborACKValid & bit()) {
            // Look for valid touches, signified by any edge on the touch toggle bit
            
            if ((neighbors[0] ^ ack->neighbors[0]) & NB0_FLAG_TOUCH) {
                Atomic::SetLZ(Event::touchCubes, id());
                Event::setPending(Event::TOUCH);
            }

        } else {
            Atomic::SetLZ(neighborACKValid, id());
        }

        // Store the raw state
        neighbors[0] = ack->neighbors[0];
        neighbors[1] = ack->neighbors[1];
        neighbors[2] = ack->neighbors[2];
        neighbors[3] = ack->neighbors[3];
    }
}

void CubeSlot::radioTimeout()
{
    /* XXX: Disconnect this cube */
}

void CubeSlot::paintCubes(_SYSCubeIDVector cv)
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
        instances[id].waitForPaint();
        waitVec ^= Intrinsic::LZ(id);
    }

    SysTime::Ticks timestamp = SysTime::ticks();

    _SYSCubeIDVector paintVec = cv;
    while (paintVec) {
        _SYSCubeID id = Intrinsic::CLZ(paintVec);
        instances[id].triggerPaint(timestamp);
        paintVec ^= Intrinsic::LZ(id);
    }
}

void CubeSlot::finishCubes(_SYSCubeIDVector cv)
{
    while (cv) {
        _SYSCubeID id = Intrinsic::CLZ(cv);
        instances[id].waitForFinish();
        cv ^= Intrinsic::LZ(id);
    }
}

void CubeSlot::waitForPaint()
{
    /*
     * Wait until we're allowed to do another paint. Since our
     * rendering is usually not fully synchronous, this is not nearly
     * as strict as waitForFinish()!
     */

    for (;;) {
        Atomic::Barrier();
        SysTime::Ticks now = SysTime::ticks();

        // Watchdog expired? Give up waiting.
        if (now > (paintTimestamp + fpsLow))
            break;

        // Wait for minimum frame rate AND for pending renders
        if (now > (paintTimestamp + fpsHigh)
            && pendingFrames <= fpMax)
            break;

        Radio::halt();
    }
}

void CubeSlot::waitForFinish()
{
    /*
     * Wait until all previous rendering has finished, and all of VRAM
     * has been updated over the radio.  Does *not* wait for any
     * minimum frame rate. If no rendering is pending, we return
     * immediately.
     *
     * Continuous rendering is turned off, if it was on.
     */

    uint8_t flags = VRAM::peekb(*vbuf, offsetof(_SYSVideoRAM,flags));
    if (flags & _SYS_VF_CONTINUOUS) {
        /*
         * If we were in continuous rendering mode, pendingFrames isn't
         * exact; it's more of a running accumulator. In that case, we have
         * to turn off continuous rendering mode, flush everything over the
         * radio, then do one Paint(). This is what paintSync() does.
         * So, at this point we just need to reset pendingFrames to zero
         * and turn off continuous mode.
         */
         
        pendingFrames = 0;

        VRAM::pokeb(*vbuf, offsetof(_SYSVideoRAM, flags), flags & ~_SYS_VF_CONTINUOUS);
        VRAM::unlock(*vbuf);
    }
     
    for (;;) {
        Atomic::Barrier();
        SysTime::Ticks now = SysTime::ticks();

        if (now > (paintTimestamp + fpsLow)) {
            // Watchdog expired. Give up waiting, and forcibly reset pendingFrames.
            pendingFrames = 0;
            break;
        }

        if (pendingFrames <= 0 && (vbuf == 0 || vbuf->cm32 == 0)) {
            // No pending renders or VRAM updates
            break;
        }

        Radio::halt();
    }
}

void CubeSlot::triggerPaint(SysTime::Ticks timestamp)
{
    _SYSAssetGroup *group = loadGroup;
        
    if (vbuf) {
        uint8_t flags = VRAM::peekb(*vbuf, offsetof(_SYSVideoRAM, flags));
        int32_t pending = Atomic::Load(pendingFrames);
        int32_t newPending = pending;

        /*
         * Keep pendingFrames above the lower limit. We make this
         * adjustment lazily, rather than doing it from inside the
         * ISR.
         */
        if (pending < fpMin)
            newPending = fpMin;

        /*
         * Count all requested paint operations, so that we can
         * loosely match them with acknowledged frames. This isn't a
         * strict 1:1 mapping, but it's used to close the loop on
         * repaint speed.
         *
         * We don't want to unlock() until we're actually ready to
         * start transmitting data over the radio, so we'll detect new
         * frames by either checking for bits that are already
         * unlocked (in needPaint) or bits that are pending unlock.
         */
        uint32_t needPaint = vbuf->needPaint | vbuf->cm32next;
        if (needPaint)
            newPending++;

        /*
         * We turn on continuous rendering only when we're doing a
         * good job at keeping the cube busy continuously, as measured
         * using our pendingFrames counter. We have some hysteresis,
         * so that continuous rendering is only turned off once the
         * cube is clearly pulling ahead of our ability to provide it
         * with frames.
         *
         * We only allow continuous rendering when we aren't downloading
         * assets to this cube. Continuous rendering makes flash downloading
         * extremely slow- flash is strictly lower priority than graphics
         * on the cube, but continuous rendering asks the cube to render
         * graphics as fast as possible.
         */

        if (group && !(group->doneCubes & bit())) {
            flags &= ~_SYS_VF_CONTINUOUS;
        } else {
            if (newPending >= fpContinuous)
                flags |= _SYS_VF_CONTINUOUS;
            if (newPending <= fpSingle)
                flags &= ~_SYS_VF_CONTINUOUS;
        }
                
        /*
         * If we're not using continuous mode, each frame is triggered
         * explicitly by toggling a bit in the flags register.
         *
         * We can always trigger a render by setting the TOGGLE bit to
         * the inverse of framePrevACK's LSB. If we don't know the ACK data
         * yet, we have to punt and set continuous mode for now.
         */
        if (!(flags & _SYS_VF_CONTINUOUS) && needPaint) {
            if (frameACKValid & bit()) {
                flags &= ~_SYS_VF_TOGGLE;
                if (!(framePrevACK & 1))
                    flags |= _SYS_VF_TOGGLE;
            } else {
                flags |= _SYS_VF_CONTINUOUS;
            }
        }

        /*
         * Atomically apply our changes to pendingFrames.
         */
        Atomic::Add(pendingFrames, newPending - pending);

        /*
         * Now we're ready to set the ISR loose on transmitting this frame over the radio.
         */
        VRAM::pokeb(*vbuf, offsetof(_SYSVideoRAM, flags), flags);
        VRAM::unlock(*vbuf);
        vbuf->needPaint = 0;
    }

    /*
     * We must always update paintTimestamp, even if this turned out
     * to be a no-op. An application which makes no changes to VRAM
     * but just calls paint() in a tight loop should iterate at the
     * 'fastPeriod' defined above.
     */
    paintTimestamp = timestamp;
}
