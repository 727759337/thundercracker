/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <sifteo.h>
#include <math.h>
#include "assets.gen.h"

using namespace Sifteo;

static Cube cube(0);

struct AffineMatrix {
    /**
     * An augmented 3x2 matrix, for doing 2D affine transforms.
     *
     *  [ xx  yx  cx ]
     *  [ xy  yy  cy ]
     *  [  0   0   1 ]
     *
     * The way we use affine transforms for background scaling are
     * very similiar to the mechanism used by the GameBoy Advance
     * PPU. There's a great tutorial on this at:
     *
     * http://www.coranac.com/tonc/text/affine.htm
     */

    float cx, cy;
    float xx, xy;
    float yx, yy;

    AffineMatrix() {}

    AffineMatrix(float _xx, float _yx, float _cx,
                 float _xy, float _yy, float _cy)
        : cx(_cx), cy(_cy), xx(_xx),
          xy(_xy), yx(_yx), yy(_yy) {}
    
    static AffineMatrix identity() {
        return AffineMatrix(1, 0, 0,
                            0, 1, 0);
    }

    static AffineMatrix scaling(float s) {
        float inv_s = 1.0f / s;
        return AffineMatrix(inv_s, 0, 0,
                            0, inv_s, 0);
    }

    static AffineMatrix translation(float x, float y) {
        return AffineMatrix(1, 0, x,
                            0, 1, y);
    }

    static AffineMatrix rotation(float angle) {
        float s = sinf(angle);
        float c = cosf(angle);
        return AffineMatrix(c, -s, 0,
                            s, c, 0);
    }

    void operator*= (const AffineMatrix &m) {
        AffineMatrix n;

        n.cx = xx*m.cx + yx*m.cy + cx;
        n.cy = xy*m.cx + yy*m.cy + cy;
        n.xx = xx*m.xx + yx*m.xy;
        n.xy = xy*m.xx + yy*m.xy;
        n.yx = xx*m.yx + yx*m.yy;
        n.yy = xy*m.yx + yy*m.yy;

        *this = n;
    }

    void translate(float x, float y) {
        *this *= translation(x, y);
    }

    void rotate(float angle) {
        *this *= rotation(angle);
    }
    
    void scale(float s) {
        *this *= scaling(s);
    }
};

static void poke_index(uint16_t addr, uint16_t tile)
{
    cube.vbuf.poke(addr, ((tile << 1) & 0xFE) | ((tile << 2) & 0xFE00));
}

void siftmain()
{
    cube.vbuf.init();

    memset(cube.vbuf.sys.vram.words, 0, sizeof cube.vbuf.sys.vram.words);
    cube.vbuf.sys.vram.mode = _SYS_VM_BG2;
    cube.vbuf.sys.vram.num_lines = 128;

    for (unsigned y = 0; y < Background.height; y++)
        for (unsigned x = 0; x < Background.width; x++)
            poke_index(offsetof(_SYSVideoRAM, bg2_tiles)/2 +
                       x + y*16, Background.tiles[x + y * Background.width]);

    cube.enable();
    cube.loadAssets(GameAssets);

    unsigned frame = 0;

    while (1) {
        AffineMatrix m = AffineMatrix::identity();
        
        /*
         * Build an affine transformation for this frame
         */

        m.translate(Background.width * 8 / 2,
                    Background.height * 8 / 2);

        m.scale(fabs(sinf(frame * 0.004) * 2));
        m.rotate(frame * 0.01);

        m.translate(-64, -64);

        /*
         * Orient the screen such that the X axis is major.
         */

        if (fabs(m.xy) > fabs(m.xx)) {
            cube.vbuf.pokeb(offsetof(_SYSVideoRAM, flags),
                            cube.vbuf.peekb(offsetof(_SYSVideoRAM, flags))
                            | _SYS_VF_XY_SWAP);
            m *= AffineMatrix(0, 1, 0,
                              1, 0, 0);
        } else {
            cube.vbuf.pokeb(offsetof(_SYSVideoRAM, flags),
                            cube.vbuf.peekb(offsetof(_SYSVideoRAM, flags))
                            & ~_SYS_VF_XY_SWAP);
        }

        /*
         * Convert the matrix to fixed-point
         */

        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.cx)/2, 0x100 * m.cx);
        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.cy)/2, 0x100 * m.cy);
        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.xx)/2, 0x100 * m.xx);
        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.xy)/2, 0x100 * m.xy);
        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.yx)/2, 0x100 * m.yx);
        cube.vbuf.poke(offsetof(_SYSVideoRAM, bg2_affine.yy)/2, 0x100 * m.yy);

        System::paint();
        frame++;
    }
}
