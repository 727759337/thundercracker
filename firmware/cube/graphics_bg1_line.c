/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * Optimized scanline renderer for the BG0 + BG1 (configurable overlay) mode.
 *
 * This is implemented with a rather complicated unrolled state machine that
 * has a separate instance for each of the 8 possible inter-layer alignments.
 * It as much as possible processes pixels in wide bursts. Chroma-keying is
 * by far our slowest remaining operation, and we optimize that using an RLE
 * scheme that relies on End Of Line markers that Stir leaves for us.
 */

#include "graphics_bg1.h"
#include "hardware.h"
 
#define R_SCRATCH       r0
#define R_X_WRAP        r1
#define R_BG0_ADDR      r3
#define R_BG1_ADDR      r4
#define R_LOOP_COUNT    r5

#define BG1_LOOP(lbl)                                           __endasm; \
    __asm djnz    R_LOOP_COUNT, lbl                             __endasm; \
    __asm ret                                                   __endasm; \
    __asm

#define BG1_NEXT_BIT()                                          __endasm; \
    __asm mov   _MD0, b                                         __endasm; \
    __asm mov   _ARCON, #0x21                                   __endasm; \
    __asm

#define BG1_UPDATE_BIT()                                        __endasm; \
    __asm mov   b, _MD0                                         __endasm; \
    __asm
    
#define BG1_JB(lbl)                                             __endasm; \
    __asm jb    b.0, lbl                                        __endasm; \
    __asm

#define BG1_JNB(lbl)                                            __endasm; \
    __asm jnb    b.0, lbl                                       __endasm; \
    __asm

#define BG1_JB_L(lbl)                                           __endasm; \
    __asm jnb    b.0, (.+6)                                     __endasm; \
    __asm ljmp   lbl                                            __endasm; \
    __asm

#define BG1_JNB_L(lbl)                                          __endasm; \
    __asm jb     b.0, (.+6)                                     __endasm; \
    __asm ljmp   lbl                                            __endasm; \
    __asm
    
#define CHROMA_PREP()                                           __endasm; \
    __asm mov   a, #_SYS_CHROMA_KEY                             __endasm; \
    __asm

#define CHROMA_J_OPAQUE(lbl)                                    __endasm; \
    __asm cjne  a, BUS_PORT, lbl                                __endasm; \
    __asm
    
// Next BG0 tile (while in BG0 state)
#define ASM_BG0_NEXT(lbl)                                       __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm ASM_X_WRAP_CHECK(lbl)                                 __endasm; \
    __asm ADDR_FROM_DPTR(_DPL)                                  __endasm; \
    __asm mov   ADDR_PORT, R_BG0_ADDR                           __endasm; \
    __asm

// Next BG0 tile (while in BG1 state)
// Can't use ASM_X_WRAP_CHECK here, since we need to redo CHROMA_PREP
// if we end up having to do a wrap, since the wrap_adjust routine
// will trash ACC.
#define ASM_BG0_NEXT_FROM_BG1(l2)                               __endasm; \
    __asm dec   _DPS                                            __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm inc   _DPS                                            __endasm; \
    __asm djnz  R_X_WRAP, l2                                    __endasm; \
    __asm lcall _vm_bg0_x_wrap_adjust                           __endasm; \
    __asm CHROMA_PREP()                                         __endasm; \
    __asm l2:                                                   __endasm; \
    __asm

// Next BG1 tile (while using DPTR1)
#define ASM_BG1_NEXT()                                          __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm inc   dptr                                            __endasm; \
    __asm anl   _DPH1, #3                                       __endasm; \
    __asm    

// State transition to BG1 pixel
#define STATE_BG1(x)                                            __endasm; \
    __asm lcall _state_bg1_func                                 __endasm; \
    __asm add   a, #((x) * 4)                                   __endasm; \
    __asm mov   ADDR_PORT, a                                    __endasm; \
    __asm CHROMA_PREP()                                         __endasm; \
    __asm

#define STATE_BG1_0()                                           __endasm; \
    __asm lcall _state_bg1_func                                 __endasm; \
    __asm mov   ADDR_PORT, a                                    __endasm; \
    __asm CHROMA_PREP()                                         __endasm; \
    __asm

static void state_bg1_func(void) __naked {
    __asm
        mov    _DPS, #1
        ADDR_FROM_DPTR(_DPL1)
        mov    a, R_BG1_ADDR
        ret
    __endasm ;
}       

// State transition to BG0 pixel, at nonzero X offset
#define STATE_BG0(x)                                            __endasm; \
    __asm lcall _state_bg0_func                                 __endasm; \
    __asm add   a, #((x) * 4)                                   __endasm; \
    __asm mov   ADDR_PORT, a                                    __endasm; \
    __asm

#define STATE_BG0_0()                                           __endasm; \
    __asm lcall _state_bg0_func                                 __endasm; \
    __asm mov   ADDR_PORT, a                                    __endasm; \
    __asm
    
static void state_bg0_func(void) __naked {
    __asm
        mov    _DPS, #0
        ADDR_FROM_DPTR(_DPL)
        mov    a, R_BG0_ADDR
        ret
    __endasm ;
}

/*
 * Eek, this little macro does a lot of things.
 *
 *   1. Addresses a pixel from BG1
 *   2. Does a chroma-key test
 *   3. If opaque, moves to the next BG1 pixel
 *   4. If transparent, draws a pixel from BG0
 *   5. Tests the EOL bit. If set, we draw the rest of the tile from BG0.
 *
 * This is also pretty space-critical, since we have 64 instances
 * of this macro. One per X-pixel per X-alignment. So, a lot of the
 * lower-bandwidth work is handled by function calls. Unfortunately
 * a lot needs to be placed here, just because of the explosion of
 * different combinations we've created.
 *
 * IMPORTANT: "x0" is the current state for BG0, within this pixel.
 *            "x1" is the _next_ state for BG1, after this pixel is done.
 */
 
#define CHROMA_BG1_BG0(l1,l2, x0,x1)                            __endasm; \
    __asm CHROMA_J_OPAQUE(l1)                                   __endasm; \
    __asm   STATE_BG0(x0)                                       __endasm; \
    __asm   lcall _addr_inc4                                    __endasm; \
    __asm   STATE_BG1(x1)                                       __endasm; \
    __asm   sjmp l2                                             __endasm; \
    __asm l1:                                                   __endasm; \
    __asm   ASM_ADDR_INC4()                                     __endasm; \
    __asm l2:                                                   __endasm; \
    __asm

#define CHROMA_BG1_BG0_EOL(l1,l2,l3, x0,x1, eolC,eolJ)          __endasm; \
    __asm CHROMA_J_OPAQUE(l1)                                   __endasm; \
    __asm   orl   ADDR_PORT, #2                                 __endasm; \
    __asm   jnb   BUS_PORT.6, l3                                __endasm; \
    __asm   STATE_BG0(x0)                                       __endasm; \
    __asm   lcall eolC                                          __endasm; \
    __asm   ljmp  eolJ                                          __endasm; \
    __asm l3:                                                   __endasm; \
    __asm   STATE_BG0(x0)                                       __endasm; \
    __asm   lcall _addr_inc4                                    __endasm; \
    __asm   STATE_BG1(x1)                                       __endasm; \
    __asm   sjmp l2                                             __endasm; \
    __asm l1:                                                   __endasm; \
    __asm   ASM_ADDR_INC4()                                     __endasm; \
    __asm l2:                                                   __endasm; \
    __asm

#define CHROMA_BG1_TARGET(lbl)                                  __endasm; \
    __asm mov   _DPS, #1                                        __endasm; \
    __asm sjmp  lbl                                             __endasm; \
    __asm

    
static void vm_bg0_bg1_tiles_fast_p0(void) __naked
{
    __asm

10$:    BG1_JB(1$)
        lcall   _addr_inc32
11$:    BG1_NEXT_BIT() ASM_BG0_NEXT(9$) BG1_UPDATE_BIT() BG1_LOOP(10$)

1$:     STATE_BG1_0()

        CHROMA_BG1_BG0_EOL(30$,40$,50$, 0,1, _addr_inc32, 13$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 1,2, _addr_inc28, 13$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 2,3, _addr_inc24, 13$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 3,4, _addr_inc20, 13$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 4,5, _addr_inc16, 13$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 5,6, _addr_inc12, 13$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 6,7, _addr_inc8,  13$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 7,0, _addr_inc4,  13$)
        
12$:    ASM_BG1_NEXT() STATE_BG0_0() ljmp 11$
13$:    CHROMA_BG1_TARGET(12$)

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p1(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        ASM_ADDR_INC4()
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc28
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(7) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 1,1, _addr_inc28, 16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 2,2, _addr_inc24, 16$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 3,3, _addr_inc20, 16$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 4,4, _addr_inc16, 16$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 5,5, _addr_inc12, 16$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 6,6, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 7,7, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0(37$,47$, 0,0)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(1) ljmp 12$
16$:    STATE_BG1(7) sjmp 17$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p2(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc8
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc24
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(6) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 2,1, _addr_inc24, 16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 3,2, _addr_inc20, 16$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 4,3, _addr_inc16, 16$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 5,4, _addr_inc12, 16$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 6,5, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 7,6, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 0,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 1,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(2) ljmp 12$
16$:    STATE_BG1(6) ljmp 17$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p3(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc12
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc20
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(5) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 3,1, _addr_inc20, 16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 4,2, _addr_inc16, 16$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 5,3, _addr_inc12, 16$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 6,4, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 7,5, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 0,6, _addr_inc12, 6$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 1,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 2,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(3) ljmp 12$
16$:    STATE_BG1(5) ljmp 17$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p4(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc16
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc16
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(4) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 4,1, _addr_inc16, 16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 5,2, _addr_inc12, 16$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 6,3, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 7,4, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 0,5, _addr_inc16, 6$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 1,6, _addr_inc12, 6$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 2,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 3,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(4) ljmp 12$
16$:    STATE_BG1(4) ljmp 17$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p5(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc20
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc12
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(3) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 5,1, _addr_inc12, 16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 6,2, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 7,3, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 0,4, _addr_inc20, 6$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 1,5, _addr_inc16, 6$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 2,6, _addr_inc12, 6$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 3,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 4,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(5) ljmp 12$
16$:    STATE_BG1(3) ljmp 17$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p6(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc24
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    lcall   _addr_inc8
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(2) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0_EOL(30$,40$,50$, 6,1, _addr_inc8,  16$)
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 7,2, _addr_inc4,  16$)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 0,3, _addr_inc24, 6$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 1,4, _addr_inc20, 6$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 2,5, _addr_inc16, 6$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 3,6, _addr_inc12, 6$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 4,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 5,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(6) ljmp 12$
16$:    STATE_BG1(2) ljmp 17$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast_p7(void) __naked
{
    __asm
    
        BG1_JB(14$)
    
10$:    BG1_NEXT_BIT()
        lcall   _addr_inc28
        BG1_UPDATE_BIT() BG1_JB_L(1$)
12$:    ASM_ADDR_INC4()
        ASM_BG0_NEXT(9$) BG1_LOOP(10$)
        
14$:    STATE_BG1(1) ljmp 13$
1$:     STATE_BG1_0()
        
        CHROMA_BG1_BG0(30$,40$, 7,1)
        
17$:    ASM_BG0_NEXT_FROM_BG1(8$) BG1_LOOP(13$)
13$:
        CHROMA_BG1_BG0_EOL(31$,41$,51$, 0,2, _addr_inc28, 6$)
        CHROMA_BG1_BG0_EOL(32$,42$,52$, 1,3, _addr_inc24, 6$)
        CHROMA_BG1_BG0_EOL(33$,43$,53$, 2,4, _addr_inc20, 6$)
        CHROMA_BG1_BG0_EOL(34$,44$,54$, 3,5, _addr_inc16, 6$)
        CHROMA_BG1_BG0_EOL(35$,45$,55$, 4,6, _addr_inc12, 6$)
        CHROMA_BG1_BG0_EOL(36$,46$,56$, 5,7, _addr_inc8,  6$)
        CHROMA_BG1_BG0_EOL(37$,47$,57$, 6,0, _addr_inc4,  6$)
        
18$:    BG1_NEXT_BIT() ASM_BG1_NEXT() BG1_UPDATE_BIT() BG1_JB_L(1$)
        STATE_BG0(7) ljmp 12$
6$:     STATE_BG1_0() sjmp 18$

    __endasm ;
}

static void vm_bg0_bg1_tiles_fast(void)
{
    /*
     * Render R_LOOP_COUNT tiles from bg0, quickly, with bg1 overlaid
     * at the proper panning offset from x_bg1_offset.
     *
     * Instead of using a jump table here, we use a small binary tree.
     * There's no way to do a jump table without trashing DPTR, and this
     * is cheaper than saving/restoring DPTR.
     */

    /*
     * Start out in BG0 state, at the beginning of the tile.
     */

    __asm
        STATE_BG0_0()
    __endasm ;

    /*
     * Use only the line portion of the Y address.
     * The pixel (column) portion is baked into these unrolled loops.
     */
     
    __asm
        mov     R_BG1_ADDR, _y_bg1_addr_l
    __endasm ;
    
    if (x_bg1_offset_bit2) {
        if (x_bg1_offset_bit1) {
            if (x_bg1_offset_bit0)
                vm_bg0_bg1_tiles_fast_p7();
            else
                vm_bg0_bg1_tiles_fast_p6();
        } else {
            if (x_bg1_offset_bit0)
                vm_bg0_bg1_tiles_fast_p5();
            else
                vm_bg0_bg1_tiles_fast_p4();
        }
    } else {
        if (x_bg1_offset_bit1) {
            if (x_bg1_offset_bit0)
                vm_bg0_bg1_tiles_fast_p3();
            else
                vm_bg0_bg1_tiles_fast_p2();
        } else {
            if (x_bg1_offset_bit0)
                vm_bg0_bg1_tiles_fast_p1();
            else
                vm_bg0_bg1_tiles_fast_p0();
        }
    }
}
 
void vm_bg0_bg1_pixels(void) __naked
{   
    /*
     * Basic pixel-by-pixel renderer for BG0 + BG1. This isn't particularly fast.
     * It's capable of rendering whole scanlines, but we only use it for partial tiles.
     *
     * Must be set up on entry:
     *   R_X_WRAP, R_BG0_ADDR, R_BG1_ADDR, R_LOOP_COUNT, DPTR, DPTR1, MDU, B
     */

    __asm
1$:
        BG1_JNB(2$)                 ; Chroma-key BG1 layer
        STATE_BG1_0()
        CHROMA_J_OPAQUE(3$)
2$:     STATE_BG0_0()               ; Draw BG0 layer
3$:     ASM_ADDR_INC4()

        ; Update BG0 accumulator
        
        mov     a, R_BG0_ADDR       ; Tentatively move ahead by 4
        add     a, #4
        mov     R_BG0_ADDR, a
        anl     a, #0x1F            ; Check for overflow
        jnz     4$
        mov     a, R_BG0_ADDR       ; Yes, overflow occurred. Compensate.
        add     a, #(0x100 - 0x20)
        mov     R_BG0_ADDR, a
        mov     _DPS, #0
        inc     dptr                ; Next BG0 tile
        inc     dptr
        ASM_X_WRAP_CHECK(5$)
4$:

        ; Update BG1 accumulator
        
        mov     a, R_BG1_ADDR       ; Tentatively move ahead by 4
        add     a, #4
        mov     R_BG1_ADDR, a
        anl     a, #0x1F            ; Check for overflow
        jnz     6$
        BG1_NEXT_BIT()              ; Yes, we switched tiles. Next bit.
        mov     a, R_BG1_ADDR       ; Overflow compensation
        add     a, #(0x100 - 0x20)
        mov     R_BG1_ADDR, a
        BG1_JNB(7$)                 ; Were we in a BG1 tile?
        mov     _DPS, #1
        ASM_BG1_NEXT()              ; Yes. Advance to the next one.
7$:     BG1_UPDATE_BIT()            ; Update MD0 cache
6$:

        djnz    R_LOOP_COUNT, 1$
        ret
    __endasm ;
}

void vm_bg0_bg1_line(void)
{
    /*
     * Top-level scanline renderer for BG0+BG1.
     * Segment the line into a fast full-tile burst, and up to two slower partial tiles.
     */
     
    __asm
        ; Load registers
     
        mov     dpl, _y_bg0_map
        mov     dph, (_y_bg0_map+1)
            
        mov     a, _y_bg0_addr_l
        add     a, _x_bg0_first_addr
        mov     R_BG0_ADDR, a
        
        mov     a, _y_bg1_addr_l
        add     a, _x_bg1_first_addr
        mov     R_BG1_ADDR, a

        mov     R_X_WRAP, _x_bg0_wrap

        BG1_UPDATE_BIT()
 
#ifdef BG1_SLOW_AND_STEADY
        ; Debugging only: Use the pixel renderer for everything
        mov     R_LOOP_COUNT, #LCD_WIDTH
        lcall   _vm_bg0_bg1_pixels
#else
        
        ; Render a partial tile at the beginning of the line, if we have one
        
        mov     a, _x_bg0_first_w
        jb      acc.3, 1$
        mov     R_LOOP_COUNT, a
        lcall   _vm_bg0_bg1_pixels
        
        ; We did render a partial tile; there are always 15 full tiles, then another partial.
        
        mov     R_LOOP_COUNT, #15
        lcall   _vm_bg0_bg1_tiles_fast
        mov     _DPS, #0

        mov     a, _y_bg1_addr_l
        add     a, _x_bg1_last_addr
        mov     R_BG1_ADDR, a
        mov     R_LOOP_COUNT, _x_bg0_last_w
        lcall   _vm_bg0_bg1_pixels
        sjmp    2$
        
        ; No partial tile? We are doing a fully aligned burst of 16 tiles.
1$:
        mov     R_LOOP_COUNT, #16
        lcall    _vm_bg0_bg1_tiles_fast       

#endif  // !BG1_SLOW_AND_STEADY
     
        ; Cleanup. Our renderers might not switch back to DPTR.
2$:       
        mov     _DPS, #0
        
    __endasm ;
}
