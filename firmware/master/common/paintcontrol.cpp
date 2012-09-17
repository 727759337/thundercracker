/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "paintcontrol.h"
#include "tasks.h"
#include "radio.h"
#include "vram.h"
#include "cube.h"
#include "systime.h"
#include "assetslot.h"
#include "assetloader.h"

#ifdef SIFTEO_SIMULATOR
#   include "system.h"
#   include "system_mc.h"
#   define PAINT_LOG(_x)    do { if (SystemMC::getSystem()->opt_paintTrace) { LOG(_x); }} while (0)
#else
#   define PAINT_LOG(_x)
#endif

#define LOG_PREFIX  "PAINT[%d]: %6u.%03us [+%4ums] pend=%-3d flags=%08x[%c%c%c%c%c] vf=%02x[%c%c] ack=%02x lock=%08x cm16=%08x  "
#define LOG_PARAMS  cube->id(), \
                    unsigned(SysTime::ticks() / SysTime::sTicks(1)), \
                    unsigned((SysTime::ticks() % SysTime::sTicks(1)) / SysTime::msTicks(1)), \
                    unsigned((SysTime::ticks() - paintTimestamp) / SysTime::msTicks(1)), \
                    pendingFrames, \
                    vbuf ? vbuf->flags : 0xFFFFFFFF, \
                        (vbuf && (vbuf->flags & _SYS_VBF_FLAG_SYNC))        ? 's' : ' ', \
                        (vbuf && (vbuf->flags & _SYS_VBF_TRIGGER_ON_FLUSH)) ? 't' : ' ', \
                        (vbuf && (vbuf->flags & _SYS_VBF_SYNC_ACK))         ? 'a' : ' ', \
                        (vbuf && (vbuf->flags & _SYS_VBF_DIRTY_RENDER))     ? 'R' : ' ', \
                        (vbuf && (vbuf->flags & _SYS_VBF_NEED_PAINT))       ? 'P' : ' ', \
                    vbuf ? VRAMFlags(vbuf).vf : 0xFF, \
                        (vbuf && VRAMFlags(vbuf).test(_SYS_VF_TOGGLE))      ? 't' : ' ', \
                        (vbuf && VRAMFlags(vbuf).test(_SYS_VF_CONTINUOUS))  ? 'C' : ' ', \
                    cube->getLastFrameACK(), \
                    vbuf ? vbuf->lock : 0xFFFFFFFF, \
                    vbuf ? vbuf->cm16 : 0xFFFFFFFF


/*
 * This object manages the somewhat complex asynchronous rendering pipeline.
 * We try to balance fast asynchronous rendering with slower but more deliberate
 * synchronous rendering.
 *
 * Here be dragons...?
 */

/*
 * System-owned VideoBuffer flag bits.
 *
 * Some of these flags are public, defined in the ABI. So far, the range
 * 0x0000FFFF is tentatively defined for public bits, while 0xFFFF0000 is
 * for these private bits. We make no guarantee about the meaning of these
 * bits, except that it's safe to initialize them to zero.
 */

// Public bits, defined in abi.h
//      _SYS_VBF_NEED_PAINT         (1 << 0)    // Request a paint operation

#define _SYS_VBF_DIRTY_RENDER       (1 << 16)   // Still rendering changed VRAM
#define _SYS_VBF_SYNC_ACK           (1 << 17)   // Frame ACK is synchronous (pendingFrames is 0 or 1)
#define _SYS_VBF_TRIGGER_ON_FLUSH   (1 << 18)   // Trigger a paint from vramFlushed()
#define _SYS_VBF_FLAG_SYNC          (1 << 19)   // This VideoBuffer has sync'ed flags with the cube
/*
 * _SYS_VBF_UNCOND_TOGGLE is a workaround for an observed case in which,
 * while waiting for finish(), we deadlock because of an apparent failure in the
 * synchronization between a vbuf's toggle bit, and the attached cube's frameACK
 * count. Only seen on hardware thus far.
 *
 * The failure mode: pollForFinish() -> triggerPaint() sets _SYS_VBF_TRIGGER_ON_FLUSH
 * and once vramFlushed() is invoked, setToggle() is called, but because the cube
 * vbuf's toggle bit is already synchronized with its frameACK count, we don't
 * generate a new delta in the flags that would cause a packet to be sent.
 *
 * Workaround is to detect the case in which we've been waiting for more than one
 * interval between triggers with _SYS_VBF_TRIGGER_ON_FLUSH set, and then set
 * _SYS_VBF_UNCOND_TOGGLE to force an edge on the toggle signal. Still not sure
 * where the initial lack of sync is coming from.
 */
#define _SYS_VBF_UNCOND_TOGGLE      (1 << 20)   // We've stalled... flip the toggle no matter what

/*
 * Frame rate control parameters:
 *
 * fpsLow --
 *    "Minimum" frame rate. If we're waiting more than this long
 *    for a frame to render, give up. Prevents us from getting wedged
 *    if a cube stops responding, and this is necessary for some types
 *    of Finish operations, e.g. when transitioning from continuous to
 *    fully synchronous rendering.
 *
 *    So, this period needs to be as short as possible while still
 *    being above the slowest expected frame rendering duration.
 *
 *    XXX: It'd be nice if we had a way to transition from continuous to
 *         fully synchronous without this last-resort timeout. We'd need a
 *         way for the cube to confirm that it's returned to its main loop
 *         and seen the continuous flag unset in VRAM.
 *
 * fpsHigh --
 *    Maximum frame rate. Paint will always block until at least this
 *    long since the previous frame, in order to provide a global rate
 *    limit for the whole app.
 *
 * fpMax --
 *    Maximum number of pending frames to track in continuous mode.
 *    If we hit this limit, Paint() calls will block.
 *
 * fpMin --
 *    Minimum number of pending frames to track in continuous mode.
 *    If we go below this limit, we'll start ignoring acknowledgments.
 */

static const SysTime::Ticks fpsLow = SysTime::hzTicks(10);
static const SysTime::Ticks fpsHigh = SysTime::hzTicks(60);
static const int8_t fpMax = 5;
static const int8_t fpMin = -8;


void PaintControl::waitForPaint(CubeSlot *cube, uint32_t excludedTasks)
{
    /*
     * Wait until we're allowed to do another paint. Since our
     * rendering is usually not fully synchronous, this is not nearly
     * as strict as waitForFinish()!
     */

    _SYSVideoBuffer *vbuf = cube->getVBuf();

    PAINT_LOG((LOG_PREFIX "+waitForPaint\n", LOG_PARAMS));

    SysTime::Ticks now;
    for (;;) {
        Atomic::Barrier();
        now = SysTime::ticks();

        // Watchdog expired? Give up waiting.
        if (now > paintTimestamp + fpsLow) {
            PAINT_LOG((LOG_PREFIX "waitForPaint, TIMED OUT\n", LOG_PARAMS));
            break;
        }

        // Cube disconnected? Give up.
        if (!cube->isSysConnected())
            break;

        // Wait for minimum frame rate AND for pending renders
        if (now > paintTimestamp + fpsHigh && pendingFrames <= fpMax)
            break;

        Tasks::idle(excludedTasks);
    }

    /*
     * Can we opportunistically regain our synchronicity here?
     */

    if (vbuf) {
        VRAMFlags vf(vbuf);
        if (canMakeSynchronous(cube, vbuf, vf, now)) {
            makeSynchronous(cube, vbuf);
            pendingFrames = 0;
        }
    }

    PAINT_LOG((LOG_PREFIX "-waitForPaint\n", LOG_PARAMS));
}

void PaintControl::triggerPaint(CubeSlot *cube, SysTime::Ticks now)
{
    _SYSVideoBuffer *vbuf = cube->getVBuf();

    /*
     * We must always update paintTimestamp, even if this turned out
     * to be a no-op. An application which makes no changes to VRAM
     * but just calls paint() in a tight loop should iterate at the
     * 'fastPeriod' defined above.
     */
    paintTimestamp = now;

    if (!vbuf)
        return;
    if (!cube->isSysConnected())
        return;

    int32_t pending = Atomic::Load(pendingFrames);
    int32_t newPending = pending;

    PAINT_LOG((LOG_PREFIX "+triggerPaint\n", LOG_PARAMS));

    bool needPaint = (vbuf->flags & _SYS_VBF_NEED_PAINT) != 0;
    Atomic::And(vbuf->flags, ~_SYS_VBF_NEED_PAINT);

    /*
     * Keep pendingFrames above the lower limit. We make this
     * adjustment lazily, rather than doing it from inside the
     * ISR.
     */

    if (pending < fpMin)
        newPending = fpMin;

    /*
     * If we're in continuous rendering, we must count every single
     * Paint invocation for the purposes of loosely matching them with
     * acknowledged frames. This isn't a strict 1:1 mapping, but
     * it's used to close the loop on repaint speed.
     */

    if (needPaint) {
        VRAMFlags vf(vbuf);
        newPending++;

        /*
         * There are multiple ways to enter continuous mode: vramFlushed()
         * can do so while handling a TRIGGER_ON_FLUSH flag, if we aren't
         * sync'ed by then. But we can't rely on this as our only way to
         * start rendering. If userspace is just pumping data into a VideoBuffer
         * like mad, and we can't stream it out over the radio quite fast enough,
         * we may not get a chance to enter vramFlushed() very often.
         *
         * So, the primary method for entering continuous mode is still as a
         * result of TRIGGER_ON_FLUSH. But as a backup, we'll enter it now
         * if we see frames stacking up in newPending.
         */

        if (newPending >= fpMax && allowContinuous(cube)) {
            if (!vf.test(_SYS_VF_CONTINUOUS)) {
                enterContinuous(cube, vbuf, vf, now);
                vf.apply(vbuf);
            }
            newPending = fpMax;
        }

        // When the codec calls us back in vramFlushed(), trigger a render
        if (!vf.test(_SYS_VF_CONTINUOUS)) {
            /*
             * Trigger on the next flush.
             *
             * If we'be already been waiting on _SYS_VBF_TRIGGER_ON_FLUSH,
             * set an unconditional toggle so we don't wait forever.
             * See notes on _SYS_VBF_UNCOND_TOGGLE above for details.
             */
            asyncTimestamp = now;
            if (vbuf->flags & _SYS_VBF_TRIGGER_ON_FLUSH) {
                Atomic::Or(vbuf->flags, _SYS_VBF_UNCOND_TOGGLE);
            } else {
                Atomic::Or(vbuf->flags, _SYS_VBF_TRIGGER_ON_FLUSH);
            }

            // Provoke a VRAM flush, just in case this wasn't happening anyway.
            if (vbuf->lock == 0)
                VRAM::lock(*vbuf, _SYS_VA_FLAGS/2);
        }

        // Unleash the radio codec!
        VRAM::unlock(*vbuf);
    }

    // Atomically apply our changes to pendingFrames.
    Atomic::Add(pendingFrames, newPending - pending);

    PAINT_LOG((LOG_PREFIX "-triggerPaint\n", LOG_PARAMS));
}

void PaintControl::beginFinish(CubeSlot *cube)
{
    /*
     * We're about to start a _SYS_finish. Every cube has their
     * beginFinish() method invoked before any cube is polled.
     *
     * Requires a valid attached video buffer.
     */

    _SYSVideoBuffer *vbuf = cube->getVBuf();
    if (!vbuf)
        return;
    if (!cube->isSysConnected())
        return;

    PAINT_LOG((LOG_PREFIX "+finish\n", LOG_PARAMS));

    // Disable continuous rendering now, if it was on. No effect otherwise.
    VRAMFlags vf(vbuf);
    exitContinuous(cube, vbuf, vf);
    vf.apply(vbuf);
}

bool PaintControl::pollForFinish(CubeSlot *cube, SysTime::Ticks now)
{
    /*
     * Is this cube finished? This is the interior of _SYS_finish's loop,
     * which performes interleaved polling of all cubes we're trying to finish.
     *
     * Returns 'true' only when all previous rendering is known to have
     * finished, and all of VRAM has been sent over the radio. Does *not*
     * wait for any minimum frame rate. If no rendering is pending, we return
     * immediately.
     *
     * Requires a valid attached video buffer.
     */

    _SYSVideoBuffer *vbuf = cube->getVBuf();
    if (!vbuf)
        return true;
    if (!cube->isSysConnected())
        return true;

    // Things to wait for...
    const uint32_t mask = _SYS_VBF_TRIGGER_ON_FLUSH | _SYS_VBF_DIRTY_RENDER;
    uint32_t flags = Atomic::Load(vbuf->flags);

    // Already done, without any arm-twisting?
    if ((mask & flags) == 0) {
        PAINT_LOG((LOG_PREFIX "-finish: flags okay, done\n", LOG_PARAMS));
        return true;
    }

    // Has it been a while since the last trigger?
    VRAMFlags vf(vbuf);
    if (canMakeSynchronous(cube, vbuf, vf, now)) {
        makeSynchronous(cube, vbuf);

        if (flags & _SYS_VBF_DIRTY_RENDER) {
            // Still need a render. Re-trigger now, and keep waiting.

            Atomic::Or(vbuf->flags, _SYS_VBF_NEED_PAINT);
            triggerPaint(cube, now);

            PAINT_LOG((LOG_PREFIX " finish RE-TRIGGER\n", LOG_PARAMS));
            ASSERT(!VRAMFlags(vbuf).test(_SYS_VF_CONTINUOUS));
            return false;

        } else {
            // The trigger expired, and we don't need to render. We're done.

            Atomic::And(vbuf->flags, ~_SYS_VBF_TRIGGER_ON_FLUSH);

            PAINT_LOG((LOG_PREFIX "-finish: trigger expired, done\n", LOG_PARAMS));
            return true;
        }
    }

    return false;
}

void PaintControl::ackFrames(CubeSlot *cube, int32_t count)
{
    /*
     * One or more frames finished rendering on the cube.
     * Use this to update our pendingFrames accumulator.
     *
     * If we are _not_ in continuous rendering mode, and
     * we have synchronized our ACK bits with the cube's
     * TOGGLE bit, this means the frame has finished
     * rendering and we can clear the 'render' dirty bit.
     */
    
    pendingFrames -= count;

    _SYSVideoBuffer *vbuf = cube->getVBuf();
    if (vbuf) {
        VRAMFlags vf(vbuf);

        if (!vf.test(_SYS_VF_CONTINUOUS) && (vbuf->flags & _SYS_VBF_SYNC_ACK)) {
            // Render is clean
            Atomic::And(vbuf->flags, ~_SYS_VBF_DIRTY_RENDER);
        }

        // Too few pending frames? Disable continuous mode.
        if (pendingFrames < fpMin) {
            exitContinuous(cube, vbuf, vf);
            vf.apply(vbuf);
        }

        PAINT_LOG((LOG_PREFIX "ACK(%d)\n", LOG_PARAMS, count));
    }
}

bool PaintControl::vramFlushed(CubeSlot *cube)
{
    /*
     * Finished flushing VRAM out to the cubes. This is only called when
     * we've fully emptied our queue of pending radio transmissions, and
     * the cube's VRAM should match our local copy exactly.
     *
     * If we are in continuous rendering mode, this isn't really an
     * important event. But if we're in synchronous mode, this indicates
     * that everything in the VRAM dirty bit can now be tracked
     * by the RENDER dirty bit; in other words, all dirty VRAM has been
     * flushed, and we can start a clean frame rendering.
     *
     * Returns true if we may have changed VRAM contents in a way which
     * could benefit from attempting to flush additional bytes.
     */

    _SYSVideoBuffer *vbuf = cube->getVBuf();
    if (!vbuf)
        return false;
    VRAMFlags vf(vbuf);

    PAINT_LOG((LOG_PREFIX "vramFlushed\n", LOG_PARAMS));

    // We've flushed VRAM, flags are sync'ed from now on.
    Atomic::Or(vbuf->flags, _SYS_VBF_FLAG_SYNC);

    if (vbuf->flags & _SYS_VBF_TRIGGER_ON_FLUSH) {
        // Trying to trigger a render

        SysTime::Ticks now = SysTime::ticks();

        PAINT_LOG((LOG_PREFIX "TRIGGERING\n", LOG_PARAMS));

        if (vbuf->flags & _SYS_VBF_SYNC_ACK) {
            // We're sync'ed up. Trigger a one-shot render

            // Should never have SYNC_ACK set when in CONTINUOUS mode.
            ASSERT(!vf.test(_SYS_VF_CONTINUOUS));

            setToggle(cube, vbuf, vf, now);

        } else {
            /*
             * We're getting ahead of the cube. We'd like to trigger now, but
             * we're no longer in sync. So, enter continuous mode. This will
             * break synchronization, in the interest of keeping our speed up.
             */

             if (!vf.test(_SYS_VF_CONTINUOUS))
                enterContinuous(cube, vbuf, vf, now);
        }

        if (vf.apply(vbuf)) {

            // Propagate the bits...
            Atomic::Or(vbuf->flags, _SYS_VBF_DIRTY_RENDER);
            Atomic::And(vbuf->flags, ~_SYS_VBF_TRIGGER_ON_FLUSH);
            return true;
        }
    }

    return false;
}

bool PaintControl::allowContinuous(CubeSlot *cube)
{
    // Conserve cube CPU time during asset loading; don't use continuous rendering.
    return 0 == (cube->bit() & AssetLoader::getActiveCubes());
}

void PaintControl::enterContinuous(CubeSlot *cube, _SYSVideoBuffer *vbuf,
    VRAMFlags &flags, SysTime::Ticks timestamp)
{
    bool allowed = allowContinuous(cube);

    PAINT_LOG((LOG_PREFIX "enterContinuous, allowed=%d\n", LOG_PARAMS, allowed));

    // Entering continuous mode; all synchronization goes out the window.
    Atomic::And(vbuf->flags, ~_SYS_VBF_SYNC_ACK);
    Atomic::Or(vbuf->flags, _SYS_VBF_DIRTY_RENDER);

    // For now, we may be async even if the ACK byte does not indicate continuous rendering
    asyncTimestamp = timestamp;

    if (allowed) {
        flags.set(_SYS_VF_CONTINUOUS);
    } else {
        // Ugh.. can't do real synchronous rendering, but we also can't
        // use continuous rendering here. So... just flip the toggle bit
        // and hope for the best.
        flags.clear(_SYS_VF_CONTINUOUS);
        flags.toggle(_SYS_VF_TOGGLE);
    }
}

void PaintControl::exitContinuous(CubeSlot *cube, _SYSVideoBuffer *vbuf,
    VRAMFlags &flags)
{
    // Exiting continuous mode; treat this as the last trigger point.
    if (flags.test(_SYS_VF_CONTINUOUS)) {
        PAINT_LOG((LOG_PREFIX "exitContinuous\n", LOG_PARAMS));
        flags.clear(_SYS_VF_CONTINUOUS);
    }
}

void PaintControl::setToggle(CubeSlot *cube, _SYSVideoBuffer *vbuf,
    VRAMFlags &flags, SysTime::Ticks timestamp)
{
    PAINT_LOG((LOG_PREFIX "setToggle\n", LOG_PARAMS));

    asyncTimestamp = timestamp;

    if (vbuf->flags & _SYS_VBF_UNCOND_TOGGLE) {
        flags.toggle(_SYS_VF_TOGGLE);
        Atomic::And(vbuf->flags, ~_SYS_VBF_UNCOND_TOGGLE);
    } else {
        flags.setTo(_SYS_VF_TOGGLE, !(cube->getLastFrameACK() & FRAME_ACK_TOGGLE));
    }
}

void PaintControl::makeSynchronous(CubeSlot *cube, _SYSVideoBuffer *vbuf)
{
    PAINT_LOG((LOG_PREFIX "makeSynchronous\n", LOG_PARAMS));

    pendingFrames = 0;

    // We can only enter SYNC_ACK state if we know that vbuf's flags
    // match what's on real hardware. We know this after any vramFlushed().
    if (vbuf->flags & _SYS_VBF_FLAG_SYNC)
        Atomic::Or(vbuf->flags, _SYS_VBF_SYNC_ACK);
}

bool PaintControl::canMakeSynchronous(CubeSlot *cube, _SYSVideoBuffer *vbuf,
    VRAMFlags &flags, SysTime::Ticks timestamp)
{
    /*
     * Our asyncTimestamp keeps track of the last event that may
     * have broke synchronization: a frame trigger, entering
     * continuous mode, or setting up a trigger-on-flush.
     *
     * We need this event to be far enough in the past that we can be
     * sure that it's had an effect on the cube already. We also need
     * To not currently be in continuous mode, and to have received
     * acknowledgment that we've exited continuous mode.
     */

    return !flags.test(_SYS_VF_CONTINUOUS)
        && !(cube->getLastFrameACK() & FRAME_ACK_CONTINUOUS)
        && timestamp > asyncTimestamp + fpsLow;
}

bool VRAMFlags::apply(_SYSVideoBuffer *vbuf)
{
    // Atomic update via XOR.
    uint8_t x = vf ^ vfPrev;

    if (x) {
        // Lock flags = 0, don't mark the "needs paint" flag.
        VRAM::xorb(*vbuf, offsetof(_SYSVideoRAM, flags), x, 0);
        VRAM::unlock(*vbuf);
        vfPrev = vf;
        return true;
    }

    return false;
}
