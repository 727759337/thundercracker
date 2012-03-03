/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Implementations for all syscall handlers.
 *
 * This is our front line of defense against buggy or malicious game
 * code, so here is where we need to carefully validate all input
 * values. The object implementations past this level don't
 * necessarily validate their input.
 */

#include <math.h>
#include <sifteo/machine.h>
#include <sifteo/abi.h>
#include "radio.h"
#include "cubeslots.h"
#include "cube.h"
#include "vram.h"
#include "neighbors.h"
#include "accel.h"
#include "audiomixer.h"
#include "prng.h"
#include "svmmemory.h"
#include "svmruntime.h"
#include "event.h"

extern "C" {

// TODO: implement!
void _SYS_ret() {

}

// XXX: floating point support stubbed out for now
uint32_t _SYS_add_f32() {
    return 0;
}

uint32_t _SYS_add_f64() {
    return 0;
}

uint32_t _SYS_sub_f32() {
    return 0;
}

uint32_t _SYS_sub_f64() {
    return 0;
}

uint32_t _SYS_mul_f32() {
    return 0;
}

uint32_t _SYS_mul_f64() {
    return 0;
}

uint32_t _SYS_div_f32() {
    return 0;
}

uint32_t _SYS_div_f64() {
    return 0;
}

uint32_t _SYS_fpext_f32_f64() {
    return 0;
}

uint32_t _SYS_fpround_f64_f32() {
    return 0;
}

uint32_t _SYS_fptosint_f32_i32() {
    return 0;
}

uint32_t _SYS_fptosint_f32_i64() {
    return 0;
}

uint32_t _SYS_fptosint_f64_i32() {
    return 0;
}

uint32_t _SYS_fptosint_f64_i64() {
    return 0;
}

uint32_t _SYS_fptouint_f32_i32() {
    return 0;
}

uint32_t _SYS_fptouint_f32_i64() {
    return 0;
}

uint32_t _SYS_fptouint_f64_i32() {
    return 0;
}

uint32_t _SYS_fptouint_f64_i64() {
    return 0;
}

uint32_t _SYS_sinttofp_i32_f32() {
    return 0;
}

uint32_t _SYS_sinttofp_i32_f64() {
    return 0;
}

uint32_t _SYS_sinttofp_i64_f32() {
    return 0;
}

uint32_t _SYS_sinttofp_i64_f64() {
    return 0;
}

uint32_t _SYS_uinttofp_i32_f32() {
    return 0;
}

uint32_t _SYS_uinttofp_i32_f64() {
    return 0;
}

uint32_t _SYS_uinttofp_i64_f32() {
    return 0;
}

uint32_t _SYS_uinttofp_i64_f64() {
    return 0;
}

uint32_t _SYS_oeq_f32() {
    return 0;
}

uint32_t _SYS_oeq_f64() {
    return 0;
}

uint32_t _SYS_une_f32() {
    return 0;
}

uint32_t _SYS_une_f64() {
    return 0;
}

uint32_t _SYS_oge_f32() {
    return 0;
}

uint32_t _SYS_oge_f64() {
    return 0;
}

uint32_t _SYS_olt_f32() {
    return 0;
}

uint32_t _SYS_olt_f64() {
    return 0;
}

uint32_t _SYS_ole_f32() {
    return 0;
}

uint32_t _SYS_ole_f64() {
    return 0;
}

uint32_t _SYS_ogt_f32() {
    return 0;
}

uint32_t _SYS_ogt_f64() {
    return 0;
}

uint32_t _SYS_uo_f32() {
    return 0;
}

uint32_t _SYS_uo_f64() {
    return 0;
}

uint32_t _SYS_o_f32() {
    return 0;
}

uint32_t _SYS_o_f64() {
    return 0;
}

#define MEMSET_BODY() {                                                 \
    if (SvmMemory::mapRAM(dest,                                         \
            SvmMemory::arraySize(sizeof *dest, count))) {               \
        while (count) {                                                 \
            *(dest++) = value;                                          \
            count--;                                                    \
        }                                                               \
    }                                                                   \
}

void _SYS_memset8(uint8_t *dest, uint8_t value, uint32_t count) MEMSET_BODY()
void _SYS_memset16(uint16_t *dest, uint16_t value, uint32_t count) MEMSET_BODY()
void _SYS_memset32(uint32_t *dest, uint32_t value, uint32_t count) MEMSET_BODY()

void _SYS_memcpy8(uint8_t *dest, const uint8_t *src, uint32_t count)
{
    FlashBlockRef ref;
    if (SvmMemory::mapRAM(dest, count))
        SvmMemory::copyROData(ref, dest, reinterpret_cast<SvmMemory::VirtAddr>(src), count);
}

void _SYS_memcpy16(uint16_t *dest, const uint16_t *src, uint32_t count)
{
    // Currently implemented in terms of memcpy8. We may provide a
    // separate optimized implementation of this syscall in the future.   
    _SYS_memcpy8((uint8_t*) dest, (const uint8_t*) src,
        SvmMemory::arraySize(sizeof *dest, count));
}

void _SYS_memcpy32(uint32_t *dest, const uint32_t *src, uint32_t count)
{
    // Currently implemented in terms of memcpy8. We may provide a
    // separate optimized implementation of this syscall in the future.   
    _SYS_memcpy8((uint8_t*) dest, (const uint8_t*) src,
        SvmMemory::arraySize(sizeof *dest, count));
}

int _SYS_memcmp8(const uint8_t *a, const uint8_t *b, uint32_t count)
{
    FlashBlockRef refA, refB;
    SvmMemory::VirtAddr vaA = reinterpret_cast<SvmMemory::VirtAddr>(a);
    SvmMemory::VirtAddr vaB = reinterpret_cast<SvmMemory::VirtAddr>(b);
    SvmMemory::PhysAddr paA, paB;

    while (count) {
        uint32_t chunkA = count;
        uint32_t chunkB = count;

        if (!SvmMemory::mapROData(refA, vaA, chunkA, paA))
            break;
        if (!SvmMemory::mapROData(refB, vaB, chunkB, paB))
            break;

        uint32_t chunk = MIN(chunkA, chunkB);

        while (chunk) {
            int diff = *(paA++) - *(paB++);
            if (diff)
                return diff;
            chunk--;
        }
        
        vaA += chunk;
        vaB += chunk;
        count -= chunk;
    }

    return 0;
}

uint32_t _SYS_strnlen(const char *str, uint32_t maxLen)
{
    FlashBlockRef ref;
    SvmMemory::VirtAddr va = reinterpret_cast<SvmMemory::VirtAddr>(str);
    SvmMemory::PhysAddr pa;
    
    uint32_t len = 0;

    while (len < maxLen) {
        uint32_t chunk = maxLen - len;
        if (!SvmMemory::mapROData(ref, va, chunk, pa))
            break;

        va += chunk;        
        while (len < maxLen && chunk && *pa) {
            pa++;
            len++;
            chunk--;
        }
    }
    return len;
}

void _SYS_strlcpy(char *dest, const char *src, uint32_t destSize)
{
    /*
     * Like the BSD strlcpy(), guaranteed to NUL-terminate.
     * We check the src pointer as we go, since the size is not known ahead of time.
     */
     
    if (destSize == 0 || !SvmMemory::mapRAM(dest, destSize))
        return;

    FlashBlockRef ref;
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    char *last = dest + destSize - 1;

    while (dest < last) {
        char c = SvmMemory::peek<uint8_t>(ref, srcVA);
        if (c) {
            *(dest++) = c;
            srcVA++;
        } else {
            break;
        }
    }

    // Guaranteed to NUL-terminate
    *dest = '\0';
}

void _SYS_strlcat(char *dest, const char *src, uint32_t destSize)
{
    if (destSize == 0 || !SvmMemory::mapRAM(dest, destSize))
        return;

    FlashBlockRef ref;
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    char *last = dest + destSize - 1;

    // Skip to NUL character
    while (dest < last && *dest)
        dest++;

    // Append all the bytes we can
    while (dest < last) {
        char c = SvmMemory::peek<uint8_t>(ref, srcVA);
        if (c) {
            *(dest++) = c;
            srcVA++;
        } else {
            break;
        }
    }

    // Guaranteed to NUL-termiante
    *dest = '\0';
}

void _SYS_strlcat_int(char *dest, int src, uint32_t destSize)
{
    /*
     * Integer to string conversion. Currently uses snprintf
     * (or sniprintf on embedded builds) but doesn't necessarily need to.
     */

    if (destSize == 0 || !SvmMemory::mapRAM(dest, destSize))
        return;

    char *last = dest + destSize - 1;

    // Skip to NUL character
    while (dest < last && *dest)
        dest++;

    if (dest < last)
        snprintf(dest, last-dest, "%d", src);

    // Guaranteed to NUL-termiante
    *last = '\0';
}

void _SYS_strlcat_int_fixed(char *dest, int src, unsigned width, unsigned lz, uint32_t destSize)
{
    if (destSize == 0 || !SvmMemory::mapRAM(dest, destSize))
        return;

    char *last = dest + destSize - 1;

    // Skip to NUL character
    while (dest < last && *dest)
        dest++;

    if (dest < last)
        snprintf(dest, last-dest, lz ? "%0*d" : "%*d", width, src);

    // Guaranteed to NUL-termiante
    *last = '\0';
}

void _SYS_strlcat_int_hex(char *dest, int src, unsigned width, unsigned lz, uint32_t destSize)
{
    if (destSize == 0 || !SvmMemory::mapRAM(dest, destSize))
        return;

    char *last = dest + destSize - 1;

    // Skip to NUL character
    while (dest < last && *dest)
        dest++;

    if (dest < last)
        snprintf(dest, last-dest, lz ? "%0*x" : "%*x", width, src);

    // Guaranteed to NUL-termiante
    *last = '\0';
}

int _SYS_strncmp(const char *a, const char *b, uint32_t count)
{
    FlashBlockRef refA, refB;
    SvmMemory::VirtAddr vaA = reinterpret_cast<SvmMemory::VirtAddr>(a);
    SvmMemory::VirtAddr vaB = reinterpret_cast<SvmMemory::VirtAddr>(b);
    SvmMemory::PhysAddr paA, paB;

    while (count) {
        uint32_t chunkA = count;
        uint32_t chunkB = count;

        if (!SvmMemory::mapROData(refA, vaA, chunkA, paA))
            break;
        if (!SvmMemory::mapROData(refB, vaB, chunkB, paB))
            break;

        uint32_t chunk = MIN(chunkA, chunkB);

        while (chunk) {
            uint8_t aV = *(paA++);
            uint8_t bV = *(paB++);
            int diff = aV - bV;
            if (diff)
                return diff;
            if (!aV)
                return 0;
            chunk--;
        }
        
        vaA += chunk;
        vaB += chunk;
        count -= chunk;
    }

    return 0;
}

void _SYS_sincosf(float x, float *sinOut, float *cosOut)
{
    /*
     * This syscall exists as such because it's very common, especially for
     * our game code, to compute both sine and cosine of the same angle.
     *
     * It's possible to do both operations in one step, e.g. with the
     * sincosf() function from GNU's math library.
     *
     * Right now we eschew this optimization in favor of portability,
     * but this function is in the ABI so we can optimize later without
     * breaking compatibility.
     */

    if (SvmMemory::mapRAM(sinOut, sizeof *sinOut))
        *sinOut = ::sinf(x);
    if (SvmMemory::mapRAM(cosOut, sizeof *cosOut))
        *cosOut = ::cosf(x);
}

float _SYS_fmodf(float a, float b)
{
    if (isfinite(a) && b != 0)
        return ::fmodf(a, b);
    else
        return NAN;
}

void _SYS_prng_init(struct _SYSPseudoRandomState *state, uint32_t seed)
{
    if (SvmMemory::mapRAM(state, sizeof *state))
        PRNG::init(state, seed);
}

uint32_t _SYS_prng_value(struct _SYSPseudoRandomState *state)
{
    if (SvmMemory::mapRAM(state, sizeof *state))
        return PRNG::value(state);
    return 0;
}

uint32_t _SYS_prng_valueBounded(struct _SYSPseudoRandomState *state, uint32_t limit)
{
    if (SvmMemory::mapRAM(state, sizeof *state))
        return PRNG::valueBounded(state, limit);
    return 0;
}

void _SYS_exit(void)
{
    SvmRuntime::exit();
}

void _SYS_yield(void)
{
    Radio::halt();
    Event::dispatch();
}

void _SYS_paint(void)
{
    CubeSlots::paintCubes(CubeSlots::vecEnabled);
    Event::dispatch();
}

void _SYS_finish(void)
{
    CubeSlots::finishCubes(CubeSlots::vecEnabled);
    Event::dispatch();
}

void _SYS_ticks_ns(int64_t *nanosec)
{
    if (SvmMemory::mapRAM(nanosec, sizeof *nanosec))
        *nanosec = SysTime::ticks();
}

void _SYS_solicitCubes(_SYSCubeID min, _SYSCubeID max)
{
    CubeSlots::solicitCubes(min, max);
}

void _SYS_enableCubes(_SYSCubeIDVector cv)
{
    CubeSlots::enableCubes(CubeSlots::truncateVector(cv));
}

void _SYS_disableCubes(_SYSCubeIDVector cv)
{
    CubeSlots::disableCubes(CubeSlots::truncateVector(cv));
}

void _SYS_setVideoBuffer(_SYSCubeID cid, struct _SYSVideoBuffer *vbuf)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf) && CubeSlots::validID(cid))
        CubeSlots::instances[cid].setVideoBuffer(vbuf);
}

void _SYS_loadAssets(_SYSCubeID cid, _SYSAssetGroup *group)
{
    if (SvmMemory::mapRAM(group, sizeof *group) && CubeSlots::validID(cid))
        CubeSlots::instances[cid].loadAssets(group);
}

void _SYS_getAccel(_SYSCubeID cid, struct _SYSAccelState *state)
{
    if (SvmMemory::mapRAM(state, sizeof *state) && CubeSlots::validID(cid))
        CubeSlots::instances[cid].getAccelState(state);
}

void _SYS_getNeighbors(_SYSCubeID cid, struct _SYSNeighborState *state) {
    if (SvmMemory::mapRAM(state, sizeof *state) && CubeSlots::validID(cid)) {
        NeighborSlot::instances[cid].getNeighborState(state);
    }
}

void _SYS_getTilt(_SYSCubeID cid, struct _SYSTiltState *state)
{
    if (SvmMemory::mapRAM(state, sizeof *state) && CubeSlots::validID(cid))
        AccelState::instances[cid].getTiltState(state);
}

void _SYS_getShake(_SYSCubeID cid, _SYSShakeState *state)
{
    if (SvmMemory::mapRAM(state, sizeof *state) && CubeSlots::validID(cid))
        AccelState::instances[cid].getShakeState(state);
}

void _SYS_getRawNeighbors(_SYSCubeID cid, uint8_t buf[4])
{
    // XXX: Temporary for testing/demoing
    if (SvmMemory::mapRAM(buf, sizeof buf) && CubeSlots::validID(cid))
        CubeSlots::instances[cid].getRawNeighbors(buf);
}

uint8_t _SYS_isTouching(_SYSCubeID cid)
{
    if (CubeSlots::validID(cid)) {
        return CubeSlots::instances[cid].isTouching();
    }
    return 0;
}

void _SYS_getRawBatteryV(_SYSCubeID cid, uint16_t *v)
{
    // XXX: Temporary for testing. Master firmware should give cooked battery percentage.
    if (SvmMemory::mapRAM(v, sizeof v) && CubeSlots::validID(cid))
        *v = CubeSlots::instances[cid].getRawBatteryV();
}

void _SYS_getCubeHWID(_SYSCubeID cid, _SYSCubeHWID *hwid)
{
    // XXX: Maybe temporary?

    // XXX: Right now this is only guaranteed to be known after asset downloading, since
    //      there is no code yet to explicitly request it (via a flash reset)

    if (SvmMemory::mapRAM(hwid, sizeof hwid) && CubeSlots::validID(cid))
        *hwid = CubeSlots::instances[cid].getHWID();
}

void _SYS_vbuf_init(_SYSVideoBuffer *vbuf)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::init(*vbuf);
    }
}

void _SYS_vbuf_lock(_SYSVideoBuffer *vbuf, uint16_t addr)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::truncateWordAddr(addr);
        VRAM::lock(*vbuf, addr);
    }
}

void _SYS_vbuf_unlock(_SYSVideoBuffer *vbuf)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::unlock(*vbuf);
    }
}

void _SYS_vbuf_poke(_SYSVideoBuffer *vbuf, uint16_t addr, uint16_t word)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::truncateWordAddr(addr);
        VRAM::poke(*vbuf, addr, word);
    }
}

void _SYS_vbuf_pokeb(_SYSVideoBuffer *vbuf, uint16_t addr, uint8_t byte)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::truncateByteAddr(addr);
        VRAM::pokeb(*vbuf, addr, byte);
    }
}

void _SYS_vbuf_peek(const _SYSVideoBuffer *vbuf, uint16_t addr, uint16_t *word)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::truncateWordAddr(addr);
        *word = VRAM::peek(*vbuf, addr);
    }
}

void _SYS_vbuf_peekb(const _SYSVideoBuffer *vbuf, uint16_t addr, uint8_t *byte)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        VRAM::truncateByteAddr(addr);
        *byte = VRAM::peekb(*vbuf, addr);
    }
}

void _SYS_vbuf_fill(struct _SYSVideoBuffer *vbuf, uint16_t addr,
                    uint16_t word, uint16_t count)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        while (count) {
            VRAM::truncateWordAddr(addr);
            VRAM::poke(*vbuf, addr, word);
            count--;
            addr++;
        }
    }
}

void _SYS_vbuf_write(struct _SYSVideoBuffer *vbuf, uint16_t addr, const uint16_t *src, uint16_t count)
{
    if (!SvmMemory::mapRAM(vbuf, sizeof *vbuf))
        return;

    FlashBlockRef ref;
    uint32_t bytes = SvmMemory::arraySize(sizeof *src, count);
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    SvmMemory::PhysAddr srcPA;

    while (bytes) {
        uint32_t chunk = bytes;
        if (!SvmMemory::mapROData(ref, srcVA, chunk, srcPA))
            return;

        while (chunk) {
            VRAM::truncateWordAddr(addr);
            VRAM::poke(*vbuf, addr, *reinterpret_cast<uint16_t*>(srcPA));
            chunk--;
            addr++;
            src += sizeof(uint16_t);
        }

        srcVA += chunk;
        bytes -= chunk;
    }
}

void _SYS_vbuf_writei(struct _SYSVideoBuffer *vbuf, uint16_t addr, const uint16_t *src,
                      uint16_t offset, uint16_t count)
{
    if (!SvmMemory::mapRAM(vbuf, sizeof *vbuf))
        return;

    FlashBlockRef ref;
    uint32_t bytes = SvmMemory::arraySize(sizeof *src, count);
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    SvmMemory::PhysAddr srcPA;

    while (bytes) {
        uint32_t chunk = bytes;
        if (!SvmMemory::mapROData(ref, srcVA, chunk, srcPA))
            return;

        while (chunk) {
            uint16_t index = offset + *reinterpret_cast<uint16_t*>(srcPA);
            VRAM::truncateWordAddr(addr);
            VRAM::poke(*vbuf, addr, VRAM::index14(index));
            chunk--;
            addr++;
            src += sizeof(uint16_t);
        }

        srcVA += chunk;
        bytes -= chunk;
    }
}

void _SYS_vbuf_seqi(struct _SYSVideoBuffer *vbuf, uint16_t addr, uint16_t index, uint16_t count)
{
    if (SvmMemory::mapRAM(vbuf, sizeof *vbuf)) {
        while (count) {
            VRAM::truncateWordAddr(addr);
            VRAM::poke(*vbuf, addr, VRAM::index14(index));
            count--;
            addr++;
            index++;
        }
    }
}

void _SYS_vbuf_wrect(struct _SYSVideoBuffer *vbuf, uint16_t addr, const uint16_t *src, uint16_t offset, uint16_t count,
                     uint16_t lines, uint16_t src_stride, uint16_t addr_stride)
{
    /*
     * Shortcut for a rectangular blit, comprised of multiple calls to SYS_vbuf_writei.
     * We save the pointer validation for writei, since we want to do it per-scanline anyway.
     */

    while (lines--) {
        _SYS_vbuf_writei(vbuf, addr, src, offset, count);
        src += src_stride;
        addr += addr_stride;
    }
}

void _SYS_vbuf_spr_resize(struct _SYSVideoBuffer *vbuf, unsigned id, unsigned width, unsigned height)
{
    // Address validation occurs after these calculations, in _SYS_vbuf_poke.
    // Sprite ID validation is implicit.

    uint8_t xb = -(int)width;
    uint8_t yb = -(int)height;
    uint16_t word = ((uint16_t)xb << 8) | yb;
    uint16_t addr = ( offsetof(_SYSVideoRAM, spr[0].mask_y)/2 +
                     sizeof(_SYSSpriteInfo)/2 * id );

    _SYS_vbuf_poke(vbuf, addr, word);
}

void _SYS_vbuf_spr_move(struct _SYSVideoBuffer *vbuf, unsigned id, int x, int y)
{
    // Address validation occurs after these calculations, in _SYS_vbuf_poke.
    // Sprite ID validation is implicit.

    uint8_t xb = -x;
    uint8_t yb = -y;
    uint16_t word = ((uint16_t)xb << 8) | yb;
    uint16_t addr = ( offsetof(_SYSVideoRAM, spr[0].pos_y)/2 +
                      sizeof(_SYSSpriteInfo)/2 * id );

    _SYS_vbuf_poke(vbuf, addr, word);
}

void _SYS_audio_enableChannel(struct _SYSAudioBuffer *buffer)
{
    if (SvmMemory::mapRAM(buffer, sizeof(*buffer)))
        AudioMixer::instance.enableChannel(buffer);
}

uint8_t _SYS_audio_play(const struct _SYSAudioModule *mod, _SYSAudioHandle *h, enum _SYSAudioLoopType loop)
{
    _SYSAudioModule modCopy;
    if (SvmMemory::copyROData(modCopy, mod) && SvmMemory::mapRAM(h, sizeof(*h))) {
        return AudioMixer::instance.play(&modCopy, h, loop);
    }
    return false;
}

uint8_t _SYS_audio_isPlaying(_SYSAudioHandle h)
{
    return AudioMixer::instance.isPlaying(h);
}

void _SYS_audio_stop(_SYSAudioHandle h)
{
    AudioMixer::instance.stop(h);
}

void _SYS_audio_pause(_SYSAudioHandle h)
{
    AudioMixer::instance.pause(h);
}

void _SYS_audio_resume(_SYSAudioHandle h)
{
    AudioMixer::instance.resume(h);
}

int _SYS_audio_volume(_SYSAudioHandle h)
{
    return AudioMixer::instance.volume(h);
}

void _SYS_audio_setVolume(_SYSAudioHandle h, int volume)
{
    AudioMixer::instance.setVolume(h, volume);
}

uint32_t _SYS_audio_pos(_SYSAudioHandle h)
{
    return AudioMixer::instance.pos(h);
}

void _SYS_setVector(_SYSVectorID vid, void *handler, void *context)
{
    if (vid < _SYS_NUM_VECTORS)
        Event::setVector(vid, handler, context);
}

void *_SYS_getVectorHandler(_SYSVectorID vid)
{
    if (vid < _SYS_NUM_VECTORS)
        return Event::getVectorHandler(vid);
    return NULL;
}

void *_SYS_getVectorContext(_SYSVectorID vid)
{
    if (vid < _SYS_NUM_VECTORS)
        return Event::getVectorContext(vid);
    return NULL;
}


}  // extern "C"
