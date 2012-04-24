/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "graphics.h"
#include "main.h"

static uint8_t next_ack;


void graphics_render(void) __naked
{
    /*
     * Check the toggle bit (rendering trigger), in bit 1 of
     * vram.flags. If it matches the LSB of frame_count, we have
     * nothing to do.
     *
     * This is also where we calculate the next ACK byte, which will
     * be sent back after this frame is fully rendered. The bits in this
     * byte are explained by protocol.h.
     */

    __asm

        orl     _next_ack, #FRAME_ACK_CONTINUOUS

        mov     dptr, #_SYS_VA_FLAGS
        movx    a, @dptr
        jb      acc.3, 1$               ; Check _SYS_VF_CONTINUOUS

        ; Toggle mode

        anl     _next_ack, #~FRAME_ACK_CONTINUOUS
        rr      a
        xrl     a, _next_ack            ; Compare _SYS_VF_TOGGLE with bit 0
        rrc     a
        jnc     3$                      ; Return if no toggle

        ; Increment frame counter field
1$:
        mov     a, _next_ack
        inc     a
        xrl     a, _next_ack
        anl     a, #FRAME_ACK_COUNT
        xrl     _next_ack, a

    __endasm ;

    global_busy_flag = 1;

    /*
     * Video mode jump table.
     *
     * This acts as a tail-call. The mode-specific function returns
     * on our behalf, after acknowledging the frame.
     */

    __asm
        mov     dptr, #_SYS_VA_MODE
        movx    a, @dptr
        anl     a, #_SYS_VM_MASK
        mov     dptr, #10$
        jmp     @a+dptr

        ; These redundant labels are required by the binary translator!

10$:    ljmp    _lcd_sleep      ; 0x00
        nop
11$:    ljmp    _vm_bg0_rom     ; 0x04
        nop
12$:    ljmp    _vm_solid       ; 0x08
        nop
13$:    ljmp    _vm_fb32        ; 0x0c
        nop
14$:    ljmp    _vm_fb64        ; 0x10
        nop
15$:    ljmp    _vm_fb128       ; 0x14
        nop
16$:    ljmp    _vm_bg0         ; 0x18
        nop
17$:    ljmp    _vm_bg0_bg1     ; 0x1c
        nop
18$:    ljmp    _vm_bg0_spr_bg1 ; 0x20
        nop
19$:    ljmp    _vm_bg2         ; 0x24
        nop
20$:    ljmp    _lcd_sleep      ; 0x28 (unused)
        nop
21$:    ljmp    _lcd_sleep      ; 0x2c (unused)
        nop
22$:    ljmp    _lcd_sleep      ; 0x30 (unused)
        nop
23$:    ljmp    _lcd_sleep      ; 0x34 (unused)
        nop
24$:    ljmp    _lcd_sleep      ; 0x38 (unused)
        nop
25$:    ljmp    _lcd_sleep      ; 0x3c (unused)

3$:     ret

    __endasm ;
}

void graphics_ack(void) __naked
{
    /*
     * If next_ack doesn't match RF_ACK_FRAME, send a packet.
     * This must happen only after the frame has fully rendered.
     */

    __asm

        mov     a, _next_ack
        xrl     a, (_ack_data + RF_ACK_FRAME)
        jz      1$
        xrl     (_ack_data + RF_ACK_FRAME), a
        orl     _ack_bits, #RF_ACK_BIT_FRAME
1$:
        ret

    __endasm ;
}
