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

#include "svm.h"
#include "svmmemory.h"

using namespace Svm;

uint8_t SvmMemory::userRAM[RAM_SIZE_IN_BYTES] __attribute__ ((aligned(4)));
FlashMapSpan SvmMemory::flashSeg[NUM_FLASH_SEGMENTS];


bool SvmMemory::mapRAM(VirtAddr va, uint32_t length, PhysAddr &pa)
{
    reg_t offset;
    
    if ((offset = reinterpret_cast<PhysAddr>(va) - userRAM) <= RAM_SIZE_IN_BYTES) {
        // Already a valid PA. This check is required for handling addresses
        // that result from pointer arithmetic on SP.

        pa = reinterpret_cast<PhysAddr>(va);

    } else if ((offset = (uint32_t)va - VIRTUAL_RAM_BASE) <= RAM_SIZE_IN_BYTES) {
        // Standard RAM address virtual-to-physical translation.
        // Note that 'va' may have junk in the upper 32 bits on 64-bit hosts,
        // due to overflow from 32-bit subtraction operations that have been emulated
        // in 64-bit registers.

        pa = userRAM + offset;

    } else {
        // Bad pointer
        return false;
    }

    // Check the extent of this region.
    // Note that with length==0, the address (VIRTUAL_RAM_BASE + RAM_SIZE_IN_BYTES) is valid.
    // This calculation must work securely for any possible 32-bit length value.
    return length <= (RAM_SIZE_IN_BYTES - offset);
}

bool SvmMemory::checkROData(VirtAddr va, uint32_t length)
{
    PhysAddr pa;
    STATIC_ASSERT(arraysize(flashSeg) == 2);
    return mapRAM(va, length, pa) ||
           flashSeg[0].offsetIsValid(va - SEGMENT_0_VA) ||
           flashSeg[1].offsetIsValid(va - SEGMENT_1_VA);
}

bool SvmMemory::mapROData(FlashBlockRef &ref, VirtAddr va,
    uint32_t &length, PhysAddr &pa)
{
    STATIC_ASSERT(arraysize(flashSeg) == 2);
    return mapRAM(va, length, pa) ||
           flashSeg[0].getBytes(ref, va - SEGMENT_0_VA, pa, length) ||
           flashSeg[1].getBytes(ref, va - SEGMENT_1_VA, pa, length);
}

bool SvmMemory::preload(VirtAddr va)
{
    STATIC_ASSERT(arraysize(flashSeg) == 2);
    return flashSeg[0].preloadBlock(va - SEGMENT_0_VA) ||
           flashSeg[1].preloadBlock(va - SEGMENT_1_VA);
}

void SvmMemory::validateBase(FlashBlockRef &ref, VirtAddr va,
    PhysAddr &bro, PhysAddr &brw)
{
    if (mapRAM(va, 1, bro)) {
        brw = bro;
        return;
    }

    STATIC_ASSERT(arraysize(flashSeg) == 2);
    brw = 0;
    if (!(flashSeg[0].getByte(ref, va - SEGMENT_0_VA, bro) ||
          flashSeg[1].getByte(ref, va - SEGMENT_1_VA, bro)))
        bro = 0;
}

bool SvmMemory::mapROCode(FlashBlockRef &ref, VirtAddr va, PhysAddr &pa)
{
    // Callers expect us to ignore the two LSBs and 8 MSBs. All real branch addresses
    // are 32-bit aligned, and some callers use these bits for special purposes.
    uint32_t flashOffset = (uint32_t)va & 0xfffffc;

    // Code can only execute from segment 0.
    if (!flashSeg[0].getBlock(ref, flashOffset & ~FlashBlock::BLOCK_MASK))
        return false;

    // Check against SvmValidator
    uint32_t blockOffset = flashOffset & FlashBlock::BLOCK_MASK;
    if (!ref->isCodeOffsetValid(blockOffset))
        return false;

    pa = ref->getData() + blockOffset;
    return true;
}

bool SvmMemory::copyROData(FlashBlockRef &ref,
    PhysAddr dest, VirtAddr src, uint32_t length)
{
    // RAM address
    PhysAddr srcPA;
    if (mapRAM(src, length, srcPA)) {
        memcpy(dest, srcPA, length);
        return true;
    }

    STATIC_ASSERT(arraysize(flashSeg) == 2);
    return flashSeg[0].copyBytes(ref, src - SEGMENT_0_VA, dest, length) ||
           flashSeg[1].copyBytes(ref, src - SEGMENT_1_VA, dest, length);
}

bool SvmMemory::strlcpyROData(FlashBlockRef &ref, char *dest, VirtAddr src, uint32_t destSize)
{
    char *last = dest + destSize - 1;

    while (dest < last) {
        uint8_t *p = peek<uint8_t>(ref, src);
        if (!p)
            return false;

        char c = *p;
        if (c) {
            *(dest++) = c;
            src++;
        } else {
            break;
        }
    }

    // Guaranteed to NUL-terminate
    *dest = '\0';
    return true;
}

unsigned SvmMemory::reconstructCodeAddr(const FlashBlockRef &ref, uint32_t pc)
{
    if (ref.isHeld()) {
        FlashMapSpan::ByteOffset offset;

        // Code only runs from segment 0.
        if (flashSeg[0].flashAddrToOffset(ref->getAddress(), offset)) {
            offset += pc & FlashBlock::BLOCK_MASK;
            offset += SEGMENT_0_VA;
            return offset;
        }
    }
    return 0;
}

bool SvmMemory::crcROData(FlashBlockRef &ref, VirtAddr src, uint32_t length,
     uint32_t &crc, unsigned alignment)
{
    CrcStream cs;

    cs.reset();

    while (length) {
        SvmMemory::PhysAddr pa;
        uint32_t chunk = length;
        if (!SvmMemory::mapROData(ref, src, chunk, pa))
            return false;

        src += chunk;
        length -= chunk;

        cs.addBytes(pa, chunk);
    }

    crc = cs.get(alignment);
    return true;
}
