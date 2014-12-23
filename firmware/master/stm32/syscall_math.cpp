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

/*
 * Syscalls for math library support support.
 *
 * Note that this file only implements operations which don't directly
 * map to functions in libgcc or libm.
 */

#include <math.h>
#include <sifteo/abi.h>
#include "svmmemory.h"

extern "C" {


void _SYS_sincosf(uint32_t x, float *sinOut, float *cosOut)
{
    float fX = reinterpret_cast<float&>(x);

    /*
     * This syscall is provided for cases where the sine and
     * cosine of the same angle are being computed. There are algorithms
     * which can calculate these two functions together faster than we
     * could calculate both separately, but so far we don't have an
     * implementation of this in our math library.
     *
     * So, for now, just turn this into separate sine and cosine operations.
     */

    if (SvmMemory::mapRAM(sinOut, sizeof *sinOut))
        *sinOut = sinf(fX);

    if (SvmMemory::mapRAM(cosOut, sizeof *cosOut))
        *cosOut = cosf(fX);
}

__attribute__((naked, noreturn))
uint64_t _SYS_mul_i64(uint32_t aL, uint32_t aH, uint32_t bL, uint32_t bH)
{
    /*
     * This is faster than __aeabi_lmul currently, since we know the
     * processor supports the umull instruction. Use umull to do a 32x32=64
     * multiply on the low words, then add (aH*bL)+(aL*bH) to the high word.
     *
     * This is the same instruction sequence that GCC would generate for
     * a plain 64-bit multiply with this same function signature, but the
     * split high and low portions confuse the optimizer enough that it's
     * better to just write out the assembly for this function ourselves.
     */

    asm volatile (
        "muls   r1, r2                  \n\t"
        "mla    r3, r0, r3, r1          \n\t"
        "umull  r0, r1, r2, r0          \n\t"
        "adds   r3, r3, r1              \n\t"
        "mov    r1, r3                  \n\t"
        "bx     lr                      \n\t"
    );
    __builtin_unreachable();
}

__attribute__((naked, noreturn))
int64_t _SYS_srem_i64(uint32_t aL, uint32_t aH, uint32_t bL, uint32_t bH)
{
    /*
     * Signed 64-bit remainder.
     *
     * EABI only provides a combined divide/remainder operation.
     * Call that, and move the remainder portion into r1:r0.
     */

    asm volatile (
        "push   {lr}                    \n\t"
        "bl     __aeabi_ldivmod         \n\t"
        "mov    r0, r2                  \n\t"
        "mov    r1, r3                  \n\t"
        "pop    {pc}                    \n\t"
    );
    __builtin_unreachable();
}

__attribute__((naked, noreturn))
uint64_t _SYS_urem_i64(uint32_t aL, uint32_t aH, uint32_t bL, uint32_t bH)
{
    /*
     * Unsigned 64-bit remainder.
     *
     * EABI only provides a combined divide/remainder operation.
     * Call that, and move the remainder portion into r1:r0.
     */

    asm volatile (
        "push   {lr}                    \n\t"
        "bl     __aeabi_uldivmod        \n\t"
        "mov    r0, r2                  \n\t"
        "mov    r1, r3                  \n\t"
        "pop    {pc}                    \n\t"
    );
    __builtin_unreachable();
}


}  // extern "C"
