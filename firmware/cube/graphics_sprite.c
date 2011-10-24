/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "graphics_sprite.h"

struct x_sprite_t x_spr[_SYS_SPRITES_PER_LINE];
uint8_t y_spr_line, y_spr_line_limit, y_spr_active;


void vm_bg0_spr_bg1(void)
{
    lcd_begin_frame();
    vm_bg0_setup();
    vm_bg1_setup();
    vm_spr_setup();

    do {
        if (y_spr_active) {
            if (y_bg1_empty)
                vm_bg0_spr_line();
            else
                vm_bg0_spr_bg1_line();
        } else {
            if (y_bg1_empty)
                vm_bg0_line();
            else
                vm_bg0_bg1_line();
        }
                
        vm_bg0_next();
        vm_bg1_next();
        vm_spr_next();

    } while (y_spr_line != y_spr_line_limit);
    
    lcd_end_frame();
}

void vm_spr_setup()
{
    y_spr_line = 0xFF;    // Will wrap to zero in vm_spr_next()
    y_spr_line_limit = vram.num_lines;
    
    vm_spr_next();        // Tail call to set up the first line
}

void vm_spr_next()
{
    /*
     * Iterate over the sprites in VRAM.
     *
     * Do the Y test on each sprite. For each sprite that passes the
     * Y test, set the corresponding bit in y_spr_active and init
     * the X parameters in x_spr.
     *
     * dptr: Source address, in _SYS_VA_SPR
     *   r0: Dest address, in _x_spr
     *   r1: tile index adjustment
     *   r2: Loop counter
     *   r3: Copy of _y_spr_line
     *   r4: mask_y
     *   r5: mask_x
     *   r6: Sprite Y offset
     *   r7: pos_x 
     *
     * In general, we'd like _SYSSpriteInfo to be laid out in the same
     * order that it's most convenient to read here; i.e. all Y params
     * first, then X, then tile. But it's even more important that we keep
     * X/Y values packed into the same word, so they can be atomically
     * updated by our radio protocol.
     *
     * The end result of this loop is a tightly packed array of X-coordinate
     * information for only those sprites that are visible on the current
     * scanline.
     *
     * The particular encoding we use for sprite locations is kind of cool.
     * The width/height are powers of two, and the positions are arbitrary
     * signed 8-bit ints. They are all stored negated. The arithmetic works
     * out such that the negated width/height becomes a bit mask. If you add
     * the negated position to the current raster position, you get the pixel
     * offset from the beginning of the sprite. This isn't surprising, since 
     * really you just subtracted the sprite origin from the raster position.
     *
     * The cool part: AND that with the mask. The negated mask is the same thing
     * as ~(width-1), or the inverse of a bitmask that has log2(width) bits set.
     * So, if you AND this with the adjusted position and you don't get zero,
     * that tells you that the position is outside the sprite. This is a little
     * faster than doing a normal inequality test.
     *
     * The other common test we'll perform: We want to know, for the next N pixels,
     * will the sprite show up anywhere in that region. We use this here to
     * test whether the sprite is on-screen horizontally at all, and we can use it
     * in our inner loops to test whether a sprite shows up within a particular
     * tile.
     *
     * For this test, we can start with the same mask test. That will tell us if
     * we're already in the sprite. If and only if we aren't, we can see if it's
     * coming up soon by adding N to the adjusted sprite position, and checking
     * for overflow.
     */

    __asm
        inc     _y_spr_line
        mov     _y_spr_active, #0
        mov     dptr, #_SYS_VA_SPR
        mov     r0, #_x_spr
        mov     r2, #_SYS_VRAM_SPRITES
        mov     r3, _y_spr_line

1$:
        ; Y tests

        movx    a, @dptr    ; mask_y
        jz      16$         ;   Sprite disabled? Skip 6 bytes.
        inc     dptr
        mov     r4, a
        
        movx    a, @dptr    ; mask_x
        mov     r5, a       ;   Save for later
        inc     dptr

        movx    a, @dptr    ; pos_y
        add     a, r3       ;   Add to current line
        mov     r6, a       ;   Save Y offset
        anl     a, r4       ;   Apply mask
        jnz     14$         ;   Not within Y bounds? Skip 4 bytes.
        inc     dptr
        
        movx    a, @dptr    ; pos_x
        mov     r7, a       ;   Save unmodified pos_x
        anl     a, r5       ;   Apply mask; are we already in the sprite?
        jz      4$          ;   Yes. Skip the next test
        mov     a, r7       ;   Back to unmodified pos_x
        add     a, #LCD_WIDTH
        jnc     13$         ;   Still nowhere on this line. Skip 3 bytes.
4$:     inc     dptr
          
        ; Activate sprite
        
        mov     a, r5       ; Save mask_x
        mov     @r0, a
        inc     r0

        mov     a, r7       ; Save pos_x
        mov     @r0, a
        inc     r0

        ; Calculate the index offset for the first tile on this line,
        ; accounting for any rows or columns that we are skipping.
        ; Since we support sprites up to 128x128 pixels, this just
        ; barely fits in 8 bits. This is stored in r1.
        
        mov     a, r6       ; Y offset
        swap    a           ; Pixels to tiles (>> 3)
        rl      a           
        anl     a, #0x1F

	mov	b, r5	    ; Use the mask to calculate a tile width, rounded up
	jnb	b.6, 24$    ;   <= 128 px
	jnb	b.5, 23$    ;   <= 64px
	jnb	b.4, 22$    ;   <= 32px
	jnb	b.3, 21$    ;   <= 16px
	sjmp	20$         ;   <= 8px

24$:	rl	a
23$:	rl	a
22$:	rl	a
21$:	rl	a
20$:    mov     r1, a

	; If the sprite overlaps the left side of the screen, we need to adjust
	; r1 to account for the horizontal sprite position too.

	mov  	a, r7	    ; X test
	anl	a, r5
	jnz	7$
	
        mov     a, r7       ; X offset
        swap    a           ; Pixels to tiles (>> 3)
        rl      a           
        anl     a, #0x1F
	add	a, r1       ; Add Y contribution from above
	mov	r1, a
7$:

        ; Store lat2:lat1, as the original tile index plus our 8-bit adjustment.
        ; This is an annoying 8 + 7:7 addition. Similar to what we are doing in
        ; the radio decoder, but unsigned.
        
        movx    a, @dptr    ; Save lat1 (tile low)
        add     a, r1       ; Add (adj << 1)
        mov     b.0, c      ;   Save both carry flags
        add     a, r1
        mov     b.1, c
        mov     @r0, a
        inc     dptr
        inc     r0
        
        movx    a, @dptr    ; Save lat2 (tile high)
        jnb     b.0, 5$     ;   2x first carry flag
        add     a, #2
5$:     jnb     b.1, 6$     ;   2x second carry flag
        add     a, #2
6$:     mov     @r0, a
        inc     dptr
        inc     r0
        
        ; Calculate the Y contribution to our low address byte.
        ; This is the Y pixel offset * 32
        
        mov     a, r6
        swap    a
        rl      a
        anl     a, #0xE0
        mov     @r0, a
        inc     r0
        
        ; Is our sprite buffer full? Exit now.

        inc     _y_spr_active
        mov     a, _y_spr_active
        xrl     a, #_SYS_SPRITES_PER_LINE
        jz      3$
        
2$:     djnz    r2, 1$      ; Next iteration
        sjmp    3$
        
        ; Skipped sprite, with various numbers of bytes left in the struct
                
16$:    mov     a, _DPL     ; Skip 6 bytes (entire _SYSSpriteInfo)
        add     a, #6
        mov     _DPL, a
        sjmp    2$          ; Next iteration

14$:    mov     a, _DPL     ; Skip 4 bytes
        add     a, #4
        mov     _DPL, a
        sjmp    2$          ; Next iteration
        
13$:    mov     a, _DPL     ; Skip 3 bytes
        add     a, #3
        mov     _DPL, a
        sjmp    2$          ; Next iteration

3$:
    __endasm ;
}

void vm_bg0_spr_line()
{
    // XXX: Unoptimized
    vm_bg0_spr_bg1_line();
}

void vm_bg0_spr_bg1_line()
{
    /*
     * XXX: Very slow unoptimized implementation. Using this to develop tests...
     */

    uint8_t bg0_addr = x_bg0_first_addr;
    uint8_t bg1_addr = x_bg1_first_addr;
    uint8_t bg0_wrap = x_bg0_wrap;
    uint8_t x = 128;
    
    DPTR = y_bg0_map;
    
    do {
        struct x_sprite_t __idata *s = x_spr;
        uint8_t ns = y_spr_active;

        // BG1
        if (MD0 & 1) {
            __asm
                inc     _DPS
                ADDR_FROM_DPTR(_DPL1)
                dec     _DPS
            __endasm ;
            ADDR_PORT = y_bg1_addr_l + bg1_addr;            
            if (BUS_PORT != _SYS_CHROMA_KEY)
                goto pixel_done;
        }
       
        // Sprites

        do {
            if (!(s->mask & s->pos)) {
                ADDR_PORT = s->lat1;
                CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT1;
                    
                ADDR_PORT = s->lat2;
                CTRL_PORT = CTRL_FLASH_OUT | CTRL_FLASH_LAT2;
                    
                ADDR_PORT = s->line_addr + ((s->pos & 7) << 2);
        
                if (BUS_PORT != _SYS_CHROMA_KEY)
                    goto pixel_done;

                s->pos++;

                if (!(7 & s->pos)) {
                    s->lat1 += 2;
                    if (!s->lat1)       
                        s->lat2 += 2;
                }

            } else {
                s->pos++;
            }

            s++;
        } while (--ns);
    
        // BG0
        __asm ADDR_FROM_DPTR(_DPL) __endasm;
        ADDR_PORT = y_bg0_addr_l + bg0_addr;
        
        pixel_done:
        ADDR_INC4();
        
        if (!(bg0_addr = (bg0_addr + 4) & 0x1F)) {
            DPTR_INC2();
            BG0_WRAP_CHECK();
        }
        
        if (!(bg1_addr = (bg1_addr + 4) & 0x1F)) {
            if (MD0 & 1) {
                __asm
                    inc     _DPS
                    inc     dptr
                    inc     dptr
                    dec     _DPS
                    anl     _DPH1, #3
                __endasm ;
            }
            MD0 = MD0;  // Dummy write to reset MDU
            ARCON = 0x21;
        }

        if (ns)
            do {
                if (!(s->mask & s->pos)) {
                    s->pos++;

                    if (!(7 & s->pos)) {
                        s->lat1 += 2;
                        if (!s->lat1)       
                            s->lat2 += 2;
                    }
                } else {
                    s->pos++;
                }
                
                s++;
            } while (--ns);

    } while (--x);
}


/*************************** TO REWORK ********************************************************/
 
#if 0

static void line_bg_spr0(void)
{
    uint8_t x = 15;
    uint8_t bg_wrap = x_bg_wrap;
    register uint8_t spr0_mask = lvram.sprites[0].mask_x;
    register uint8_t spr0_x = lvram.sprites[0].x + x_bg_first_w;
    uint8_t spr0_pixel_addr = (spr0_x - 1) << 2;

    DPTR = y_bg_map;

    // First partial or full tile
    ADDR_FROM_DPTR_INC();
    MAP_WRAP_CHECK();
    ADDR_PORT = y_bg_addr_l + x_bg_first_addr;
    PIXEL_BURST(x_bg_first_w);

    // There are always 15 full tiles on-screen
    do {
        if ((spr0_x & spr0_mask) && ((spr0_x + 7) & spr0_mask)) {
            // All 8 pixels are non-sprite

            ADDR_FROM_DPTR_INC();
            MAP_WRAP_CHECK();
            ADDR_PORT = y_bg_addr_l;
            addr_inc32();
            spr0_x += 8;
            spr0_pixel_addr += 32;

        } else {
            // A mixture of sprite and tile pixels.

#define SPR0_OPAQUE(i)                          \
        test_##i:                               \
            if (BUS_PORT == CHROMA_KEY)         \
                goto transparent_##i;           \
            ADDR_INC4();                        \

#define SPR0_TRANSPARENT_TAIL(i)                \
        transparent_##i:                        \
            ADDR_FROM_DPTR();                   \
            ADDR_PORT = y_bg_addr_l + (i*4);    \
            ADDR_INC4();                        \

#define SPR0_TRANSPARENT(i, j)                  \
            SPR0_TRANSPARENT_TAIL(i);           \
            ADDR_FROM_SPRITE(0);                \
            ADDR_PORT = spr0_pixel_addr + (j*4);\
            goto test_##j;                      \

#define SPR0_END()                              \
            spr0_x += 8;                        \
            spr0_pixel_addr += 32;              \
            DPTR_INC2();                        \
            MAP_WRAP_CHECK();                   \
            continue;                           \

            // Fast path: All opaque pixels in a row.

            // XXX: The assembly generated by sdcc for this loop is okayish, but
            //      still rather bad. There are still a lot of gains left to be had
            //      by using inline assembly here.

            ADDR_FROM_SPRITE(0);
            ADDR_PORT = spr0_pixel_addr;
            SPR0_OPAQUE(0);
            SPR0_OPAQUE(1);
            SPR0_OPAQUE(2);
            SPR0_OPAQUE(3);
            SPR0_OPAQUE(4);
            SPR0_OPAQUE(5);
            SPR0_OPAQUE(6);
            SPR0_OPAQUE(7);
            SPR0_END();

            // Transparent pixel jump targets

            SPR0_TRANSPARENT(0, 1);
            SPR0_TRANSPARENT(1, 2);
            SPR0_TRANSPARENT(2, 3);
            SPR0_TRANSPARENT(3, 4);
            SPR0_TRANSPARENT(4, 5);
            SPR0_TRANSPARENT(5, 6);
            SPR0_TRANSPARENT(6, 7);
            SPR0_TRANSPARENT_TAIL(7);
            SPR0_END();
        }

    } while (--x);

    // Might be one more partial tile
    if (x_bg_last_w) {
        ADDR_FROM_DPTR_INC();
        MAP_WRAP_CHECK();
        ADDR_PORT = y_bg_addr_l;
        PIXEL_BURST(x_bg_last_w);
    }

    do {
        uint8_t active_sprites = 0;

        /*
         * Per-line sprite accounting. Update all Y coordinates, and
         * see which sprites are active. (Unrolled loop here, to allow
         * calculating masks and array addresses at compile-time.)
         */

#define SPRITE_Y_ACCT(i)                                                \
        if (!(++lvram.sprites[i].y & lvram.sprites[i].mask_y)) {        \
            active_sprites |= 1 << i;                                   \
            lvram.sprites[i].addr_l += 2;                               \
        }                                                               \

        SPRITE_Y_ACCT(0);
        SPRITE_Y_ACCT(1);

        /*
         * Choose a scanline renderer
         */

        switch (active_sprites) {
        case 0x00:      line_bg(); break;
        case 0x01:      line_bg_spr0(); break;
        }
#endif
