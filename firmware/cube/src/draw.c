/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * Micah Elizabeth Scott <micah@misc.name>
 * Copyright <c> 2011 Sifteo, Inc.
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

#include <stdint.h>
#include "lcd.h"
#include "draw.h"

void draw_clear()
{
    /*
     * Clear all of VRAM (1 kB) except for mode and flags.
     *
     * We want to avoid resetting those fields continuously,
     * so we have a smoother transition from disconnected
     * to connected mode.
     */

    __asm
        mov     dptr, #0
1$:     clr     a

        movx    @dptr, a
        inc     dptr

        mov     a, #(_SYS_VA_MODE >> 8)
        cjne    a, dph, 1$
        mov     a, #(_SYS_VA_MODE & 0xFF)
        cjne    a, dpl, 1$

    __endasm ;
}

void draw_image(const __code uint8_t *image)
{
    // Image in DPTR, Dest address in DPTR1.
    image = image;
 
    __asm

        ; Read header

        clr     a               ; Width
        movc    a, @a+DPTR
        mov     r1, a
        inc     dptr

        clr     a               ; Height
        movc    a, @a+DPTR
        mov     r2, a
        inc     dptr

        clr     a               ; MSB / Mode
        movc    a, @a+DPTR
        mov     r3, a
        inc     dptr

        
1$:                             ; Loop over all lines
        mov     ar0, r1         ; Loop over tiles in this line
2$:

        clr     a               ; Read LSB
        movc    a, @a+DPTR
        inc     dptr
        clr     c               ; Tile index to low 7 bits, with extra bit in C
        rlc     a

        mov     _DPS, #1        ; Write low byte to destination
        movx    @dptr, a
        inc     dptr

        mov     a, r3           ; Test mode: Is the source 8-bit or 16-bit?
        inc     a
        jz      3$              ; Jump if 16-bit

        ; 8-bit source

        mov     a, r3           ; Start with common MSB
        rlc     a               ; Extra bit from C
        rl      a               ; Index to 7:7
        movx    @dptr, a        ; Write to high byte
        inc     dptr
        mov     _DPS, #0        ; Next source byte
        djnz    r0, 2$          ; Next tile
        sjmp    4$

        ; 16-bit source
3$:
        mov     _DPS, #0        ; Get another source byte
        clr     a
        movc    a, @a+DPTR
        inc     dptr
        mov     _DPS, #1

        rlc     a               ; Rotate in extra bit from C
        rl      a               ; Index to 7:7
        movx    @dptr, a        ; Write to high byte
        inc     dptr
        mov     _DPS, #0        ; Next source byte

        djnz    r0, 2$          ; Next tile
4$:

        ; Next line, adjust destination address by (PITCH - width)

        clr    c
        mov    a, #_SYS_VRAM_BG0_WIDTH
        subb   a, r1
        rl     a

        add    a, _DPL1
        mov    _DPL1, a
        mov    a, _DPH1
        addc   a, #0
        mov    _DPH1, a

        djnz    r2, 1$

    __endasm ;  
}
