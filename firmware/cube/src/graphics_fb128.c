/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "graphics.h"

extern uint8_t fb128_y;

/*
 * 2-color 128x48 framebuffer mode.
 *
 * Yes, this is too small to fit the screen :)
 *
 * If num_lines > 48, this buffer will repeat itself. That effect may
 * be useful perhaps, but this is actually intended to be used for
 * split-screen or letterboxed effects, particularly variable-spacing
 * text.
 *
 * Two-entry colormap, 16 bytes per line.
 *
 * Clobbers r2-r7.
 */

static void vm_fb128_line(uint16_t ptr)
{
    ptr = ptr;
    __asm

        ; Copy colormap[0] and colormap[1] to r4-7

        mov     _DPS, #1
        mov     dptr, #_SYS_VA_COLORMAP
        movx    a, @dptr
        mov     r4, a
        inc     dptr
        movx    a, @dptr
        mov     r5, a
        inc     dptr
        movx    a, @dptr
        mov     r6, a
        inc     dptr
        movx    a, @dptr
        mov     r7, a
        mov     _DPS, #0

        mov     r2, #16         ; Loop over 16 horizontal bytes per line
4$:     movx    a, @dptr
        inc     dptr
        mov     r3, #8          ; Loop over 8 pixels per byte
5$:     rrc     a               ; Shift out one pixel
        jc      6$

        PIXEL_FROM_REGS(r4, r5) ; colormap[0]
        djnz    r3, 5$          ; Next pixel
        djnz    r2, 4$          ; Next byte
        sjmp    7$

6$:     PIXEL_FROM_REGS(r6, r7) ; colormap[1]
        djnz    r3, 5$          ; Next pixel
        djnz    r2, 4$          ; Next byte

7$:

    __endasm ;
}

void vm_fb128(void) __naked
{
    lcd_begin_frame();
    LCD_WRITE_BEGIN();

    __asm

        ; Keep line count in fb128_y

        mov     dptr, #_SYS_VA_NUM_LINES
        movx    a, @dptr
        mov     _fb128_y, a

        clr     a
        mov     r1, a               ; Line pointer in r1:r0
        mov     r0, a

1$:
        mov     dpl, r0             ; Draw line from r1:r0
        mov     dph, r1
        acall   _vm_fb128_line

        mov     a, r0               ; Add 16 bytes
        add     a, #16
        mov     r0, a
        mov     a, r1
        addc    a, #0
        cjne    a, #(_SYS_VA_COLORMAP >> 8), 2$
        clr     a                   ; Wrap at the bottom of the framebuffer
2$:
        mov     r1, a

        djnz    _fb128_y, 1$        ; Next line. Done yet?

10$:
    __endasm;

    lcd_end_frame();
    GRAPHICS_RET();
}
