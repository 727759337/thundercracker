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

#ifndef _VRAM_H
#define _VRAM_H

#include <sifteo/abi.h>
#include "machine.h"


/**
 * Utilities for accessing VRAM buffers.
 *
 * These are all operations that *could* be implemented within the
 * runtime, as usermode does have access to the VRAM buffer memory.
 * But we'd usually be doing this from inside the firmware, for
 * performance reasons. There are also a few places where we do want
 * to access VRAM directly from the firmware.
 *
 * When possible, these internal utilities mirror the ones available
 * to userspace in the VideoBuffer class. But these operate at a
 * slightly lower level. For more documentation on the semantics of
 * these operations, see the userlevel VideoBuffer class.
 */

struct VRAM {

    static const uint32_t DEFAULT_LOCK_FLAGS = _SYS_VBF_NEED_PAINT;

    static ALWAYS_INLINE uint32_t &selectCM1(_SYSVideoBuffer &vbuf, uint16_t addr) {
        ASSERT(addr < _SYS_VRAM_WORDS);
        STATIC_ASSERT((_SYS_VRAM_WORD_MASK >> 5) < arraysize(vbuf.cm1));
        return vbuf.cm1[addr >> 5];
    }

    static ALWAYS_INLINE uint32_t indexCM1(uint16_t addr) {
        ASSERT(addr < _SYS_VRAM_WORDS);
        return addr & 31;
    }

    static ALWAYS_INLINE uint32_t maskCM1(uint16_t addr) {
        return Intrinsic::LZ(indexCM1(addr));
    }

    static ALWAYS_INLINE uint32_t maskCM16(uint16_t addr) {
        ASSERT(addr < _SYS_VRAM_WORDS);
        STATIC_ASSERT((_SYS_VRAM_WORD_MASK >> 4) < 32);
        return Intrinsic::LZ(addr >> 4);
    }

    static ALWAYS_INLINE void truncateByteAddr(uint16_t &addr) {
        addr &= _SYS_VRAM_BYTE_MASK;
    }

    static ALWAYS_INLINE void truncateWordAddr(uint16_t &addr) {
        addr &= _SYS_VRAM_WORD_MASK;
    }

    static ALWAYS_INLINE void truncateWordAddr(unsigned &addr) {
        addr &= _SYS_VRAM_WORD_MASK;
    }

    static void lock(_SYSVideoBuffer &vbuf, uint16_t addr,
        uint32_t lockFlags = DEFAULT_LOCK_FLAGS)
    {
        DEBUG_LOG(("VBUF[%p]: lock at %04x, flags %08x\n", &vbuf, addr, lockFlags));
        Atomic::Or(vbuf.flags, lockFlags);
        ASSERT(addr < _SYS_VRAM_WORDS);
        vbuf.lock |= maskCM16(addr);
        Atomic::Barrier();
    }

    static void unlock(_SYSVideoBuffer &vbuf) {
        Atomic::Or(vbuf.cm16, vbuf.lock);
        vbuf.lock = 0;
    }

    static void poke(_SYSVideoBuffer &vbuf, uint16_t addr, uint16_t word,
        uint32_t lockFlags = DEFAULT_LOCK_FLAGS)
    {
        ASSERT(addr < _SYS_VRAM_WORDS);

        if (vbuf.vram.words[addr] != word) {
            lock(vbuf, addr, lockFlags);
            vbuf.vram.words[addr] = word;
            Atomic::SetLZ(selectCM1(vbuf, addr), indexCM1(addr));
        }
    }

    static void pokeb(_SYSVideoBuffer &vbuf, uint16_t addr, uint8_t byte,
        uint32_t lockFlags = DEFAULT_LOCK_FLAGS)
    {
        ASSERT(addr < _SYS_VRAM_BYTES);

        if (vbuf.vram.bytes[addr] != byte) {
            uint16_t addrw = addr >> 1;
            lock(vbuf, addrw, lockFlags);
            vbuf.vram.bytes[addr] = byte;
            Atomic::SetLZ(selectCM1(vbuf, addrw), indexCM1(addrw));
        }
    }

    static ALWAYS_INLINE uint16_t peek(const _SYSVideoBuffer &vbuf, uint16_t addr) {
        ASSERT(addr < _SYS_VRAM_WORDS);
        return vbuf.vram.words[addr];
    }

    static ALWAYS_INLINE uint8_t peekb(const _SYSVideoBuffer &vbuf, uint16_t addr) {
        ASSERT(addr < _SYS_VRAM_BYTES);
        return vbuf.vram.bytes[addr];
    }

    // Like pokeb(), but does an atomic XOR.
    static void xorb(_SYSVideoBuffer &vbuf, uint16_t addr, uint8_t byte,
        uint32_t lockFlags = DEFAULT_LOCK_FLAGS)
    {
        ASSERT(addr < _SYS_VRAM_BYTES);

        if (byte != 0) {
            uint16_t addrw = addr >> 1;
            lock(vbuf, addrw, lockFlags);
            __sync_xor_and_fetch(&vbuf.vram.bytes[addr], byte);
            Atomic::SetLZ(selectCM1(vbuf, addrw), indexCM1(addrw));
        }
    }

    static void init(_SYSVideoBuffer &vbuf) {
        vbuf.lock = 0xFFFFFFFF;
        for (unsigned i = 0; i < arraysize(vbuf.cm1); i++)
            vbuf.cm1[i] = 0xFFFFFFFF;
        vbuf.flags = DEFAULT_LOCK_FLAGS;
    }
};


/**
 * An iterator for walking the BG1 mask bitmap.
 *
 * This iterator can seek to any (x,y) location on BG1, and
 * determine what address, if any, the corresponding tile is at.
 * It supports random access, but it's optimized for sequential access.
 */

class BG1MaskIter {
public:
    BG1MaskIter(const _SYSVideoBuffer &vbuf)
        : vbuf(vbuf) {
        reset();
    }

    void reset() {
        // Reset iteration to the beginning of the bitmap
        tileAddr = firstTileAddr;
        bmpAddr = firstBmpAddr;
        bmpValue = VRAM::peek(vbuf, bmpAddr);
        bmpShift = 0;
    }

    bool next() {
        // Nowhere to go?
        if (tileAddr == lastTileAddr)
            return false;

        // Move forward by one bit
        if (bmpShift != 15) {
            if (hasTile())
                tileAddr++;
            bmpShift++;
            return true;
        }

        // Next word
        if (bmpAddr != lastBmpAddr) {
            if (hasTile())
                tileAddr++;
            bmpShift = 0;
            bmpAddr++;
            bmpValue = VRAM::peek(vbuf, bmpAddr);
            return true;
        }

        // End of iteration
        return false;
    }

    bool seek(unsigned x, unsigned y) {
        // Move the iterator to a particular (x,y) location on BG1.
        // Returns false if out of range.

        if (x > 15 || y > 15)
            return false;

        unsigned destBmpAddr = y + firstBmpAddr;
        if (destBmpAddr < bmpAddr)
            reset();
        else if (destBmpAddr == bmpAddr && x < bmpShift)
            reset();

        while (destBmpAddr != bmpAddr)
            if (!next())
                return false;
        while (bmpShift != x)
            if (!next())
                return false;

        return true;
    }

    ALWAYS_INLINE uint16_t getTileAddr() const {
        ASSERT(hasTile());
        return tileAddr;
    }

    ALWAYS_INLINE bool hasTile() const {
        // Is the current location allocated?
        return (bmpValue >> bmpShift) & 1;
    }

private:
    static const unsigned firstTileAddr = _SYS_VA_BG1_TILES / 2;
    static const unsigned lastTileAddr = firstTileAddr + _SYS_VRAM_BG1_TILES - 1;
    static const unsigned firstBmpAddr = _SYS_VA_BG1_BITMAP / 2;
    static const unsigned lastBmpAddr = firstBmpAddr + 15;
    
    const _SYSVideoBuffer &vbuf;
    uint16_t tileAddr;
    uint16_t bmpAddr;
    uint16_t bmpValue;
    uint8_t bmpShift;
};


#endif
