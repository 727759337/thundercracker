/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>
#include <string.h>

#include "vtime.h"

namespace Cube {


class LCD {
 public:
    static const unsigned WIDTH  = 128;
    static const unsigned HEIGHT = 128;

    static const unsigned FB_SIZE = WIDTH * HEIGHT;
    static const unsigned FB_MASK = FB_SIZE - 1;
    static const unsigned FB_ROW_SHIFT = 7;

    struct Pins {
        /* Configured for an 8-bit parallel bus, in 80-system mode */
        
        uint8_t   power;      // IN, active-high
        uint8_t   csx;        // IN, active-low
        uint8_t   dcx;        // IN, low=cmd high=data
        uint8_t   wrx;        // IN, rising edge
        uint8_t   rdx;        // IN, rising edge
        uint8_t   data_in;    // IN
    };

    /* 16-bit RGB 5-6-5 format */
    uint16_t fb_mem[FB_SIZE];

    void init() {
        // Framebuffer contents undefined. Simulate that...
        uint32_t i;
        for (i = 0; i < FB_SIZE; i++)
            fb_mem[i] = 31337 * (1+i);

        current_cmd = 0;
        cmd_bytecount = 0;

        xs = 0;
        xe = WIDTH - 1;
        ys = 0;
        ye = HEIGHT - 1;
        row = 0;
        col = 0;
        
        madctr = 0;
        colmod = COLMOD_18;

        mode_awake = 0;
        mode_display_on = 0;
        mode_te = 0;
        mode_power_on = 1;
        
        frame_count = 0;
        pixel_count = 0;
    }

    ALWAYS_INLINE void cycle(Pins *pins) {
        /*
         * Make lots of assumptions...
         *
         * This is pretending to be an SPFD5414-like controller, with the following settings:
         *
         *   - 8-bit parallel interface, in 80-series mode
         *   - 16-bit color depth, RGB-565 (3AH = 05)
         */
        
        if (pins->power) {
            if (!mode_power_on)
                init();
        
            if (!pins->csx && pins->wrx && !prev_wrx) {
                if (pins->dcx) {
                    /* Data write strobe */
                    data(pins->data_in);
                } else {
                    /* Command write strobe */
                    command(pins->data_in);
                }
            }
        } else {
            mode_display_on = 0;
            mode_awake = 0;
            mode_power_on = 0;
        }

        prev_wrx = pins->wrx;
    }

    uint32_t getFrameCount() {
        // Estimated number of frames.
        return frame_count;
    }
    
    uint32_t getPixelCount() {
        // Number of pixels written
        return pixel_count;
    }

    bool isVisible() {  
        return mode_awake && mode_display_on;
    }

    void pulseTE(TickDeadline &deadline) {
        if (mode_te) {
            // This runs on the GUI thread, use a lock-free timer.
            te_timestamp = deadline.setRelative(VirtualTime::usec(TE_WIDTH_US));
        }
    }

    ALWAYS_INLINE void tick(TickDeadline &deadline, CPU::em8051 *cpu) {
        /*
         * We have a separate entry point for ticking the TE timer,
         * since it really does need to happen every tick rather than
         * just when there's a graphics pin state change.
         */
    
        if (deadline.hasPassed(te_timestamp)) {
            cpu->mSFR[CTRL_PORT] &= ~CTRL_LCD_TE;
        } else {
            cpu->mSFR[CTRL_PORT] |= CTRL_LCD_TE;
            deadline.set(te_timestamp);
        }
    }

 private:
    // XXX: We have support in software for TE, but the IO pin isn't wired up currently
    static const uint8_t CTRL_LCD_TE     = 0;
    static const uint8_t CTRL_PORT       = REG_P3;

    void firstPixel() {
        // Return to start row/column
        row = ys;
        col = xs;
    }

    void applyMirroring(uint8_t flags, unsigned &row, unsigned &col) {
        row = (flags & MADCTR_MY) ? (HEIGHT - 1 - row) : row;
        col = (flags & MADCTR_MX) ? (WIDTH - 1 - col) : col;
    }

    void writePixel(uint16_t pixel) {
        unsigned vRow = row;
        unsigned vCol = col;
        uint8_t m = madctr ^ model.madctr_xor;

        if (model.order == model.MIRROR_BEFORE_SWAP)
            applyMirroring(m, vRow, vCol);

        vRow += model.row_adj;
        vCol += model.col_adj;

        if (m & MADCTR_MV)
            std::swap(vRow, vCol);

        if (model.order == model.SWAP_BEFORE_MIRROR)
            applyMirroring(m, vRow, vCol);

        unsigned addr = vCol + (vRow << FB_ROW_SHIFT);
        fb_mem[addr & FB_MASK] = pixel;

        if (++col > xe) {
            col = xs;
            if (++row > ye)
                row = ys;
        }
        
        pixel_count++;
    }

    ALWAYS_INLINE void writeByte(uint8_t b) {
        pixel_bytes[cmd_bytecount++] = b;

        switch (colmod) {

        case COLMOD_12:
            if (cmd_bytecount == 3) {
                uint8_t r1 = pixel_bytes[0] >> 4;
                uint8_t g1 = pixel_bytes[0] & 0x0F;
                uint8_t b1 = pixel_bytes[1] >> 4;

                uint8_t r2 = pixel_bytes[1] & 0x0F;
                uint8_t g2 = pixel_bytes[2] >> 4;
                uint8_t b2 = pixel_bytes[2] & 0x0F;

                cmd_bytecount = 0;

                writePixel( (r1 << 12) | ((r1 >> 3) << 11) |
                            (g1 << 7) | ((g1 >> 2) << 5) |
                            (b1 << 1) | (b1 >> 3) );
                
                writePixel( (r2 << 12) | ((r2 >> 3) << 11) |
                            (g2 << 7) | ((g2 >> 2) << 5) |
                            (b2 << 1) | (b2 >> 3) );
            }
            break;

        case COLMOD_16:
            if (cmd_bytecount == 2) {
                cmd_bytecount = 0;
                writePixel( (pixel_bytes[0] << 8) |
                            pixel_bytes[1] );
            }
            break;

        case COLMOD_18:
            if (cmd_bytecount == 3) {
                uint8_t r = pixel_bytes[0] >> 3;
                uint8_t g = pixel_bytes[1] >> 2;
                uint8_t b = pixel_bytes[2] >> 3;

                cmd_bytecount = 0;
                writePixel( (r << 11) | (g << 5) | b );
            }
            break;

        default:
            cmd_bytecount = 0;
            break;
        }
    }

    ALWAYS_INLINE void command(uint8_t op) {
        current_cmd = op;
        cmd_bytecount = 0;

        switch (op) {

        case CMD_RAMWR:
            firstPixel();
            break;

        case CMD_SWRESET:
            init();
            break;

        case CMD_SLPIN:
            mode_awake = 0;
            break;

        case CMD_SLPOUT:
            mode_awake = 1;
            break;

        case CMD_DISPOFF:
            mode_display_on = 0;
            break;

        case CMD_DISPON:
            mode_display_on = 1;

            /*
             * Current firmware issues this command at the bottom of every
             * frame, so use it as a probe to count frames. Note that RAMWR
             * is a perfectly good count in most video modes, but not in
             * modes that re-address the LCD multiple times per frame,
             * like STAMP mode.
             */
            frame_count++;
            break;

        case CMD_TEOFF:
            mode_te = 0;
            break;

        case CMD_TEON:
            mode_te = 1;
            break;

        /*
         * Look for specific magic commands in order to guess what LCD this
         * firmware build is expecting to talk to. Then, undo that firmware's
         * model-specific workarounds.
         */

        case CMD_MAGIC_TRULY:
            model.madctr_xor = MADCTR_MX | MADCTR_MY;
            model.left_margin = 0;
            model.right_margin = 0;
            model.top_margin = 32;
            model.bottom_margin = 0;
            model.order = model.SWAP_BEFORE_MIRROR;
            break;

        case CMD_MAGIC_TIANMA_HX8353:
            model.madctr_xor = MADCTR_MX | MADCTR_MY;
            model.left_margin = 0;
            model.right_margin = 0;
            model.top_margin = 0;
            model.bottom_margin = 0;
            model.order = model.SWAP_BEFORE_MIRROR;
            break;

        case CMD_MAGIC_SANTEK_ST7735R:
            model.madctr_xor = MADCTR_MX | MADCTR_MY;
            model.left_margin = 2;
            model.right_margin = 2;
            model.top_margin = 1;
            model.bottom_margin = 33;
            model.order = model.SWAP_BEFORE_MIRROR;
            break;

        case CMD_MAGIC_WnW_RM68116:
            model.madctr_xor = MADCTR_MX | MADCTR_MY;
            model.left_margin = 0;
            model.right_margin = 0;
            model.top_margin = 0;
            model.bottom_margin = 0;
            model.order = model.SWAP_BEFORE_MIRROR;
            break;
        }
    }

    ALWAYS_INLINE void data(uint8_t byte) {
        switch (current_cmd) {

        case CMD_CASET:
            switch (cmd_bytecount++) {
            case 1:  xs = byte;
            case 3:  xe = byte;
            }
            break;

        case CMD_RASET:
            switch (cmd_bytecount++) {
            case 1:  ys = byte;
            case 3:  ye = byte;
            }
            break;
            
        case CMD_MADCTR:
            madctr = byte;
            model.row_adj = (madctr & MADCTR_MY) ? -model.bottom_margin : -model.top_margin;
            model.col_adj = (madctr & MADCTR_MX) ? -model.right_margin : -model.left_margin;
            if (madctr & MADCTR_MV) {
                std::swap(model.row_adj, model.col_adj);
            }
            break;

        case CMD_COLMOD:
            colmod = byte;
            break;
            
        case CMD_RAMWR:
            writeByte(byte);
            break;
        }
    }
    
    static uint8_t clamp(uint8_t val, uint8_t min, uint8_t max) {
        if (val < min) val = min;
        if (val > max) val = max;
        return val;
    }

    static const uint8_t CMD_NOP      = 0x00;
    static const uint8_t CMD_SWRESET  = 0x01;
    static const uint8_t CMD_SLPIN    = 0x10;
    static const uint8_t CMD_SLPOUT   = 0x11;
    static const uint8_t CMD_DISPOFF  = 0x28;
    static const uint8_t CMD_DISPON   = 0x29;
    static const uint8_t CMD_CASET    = 0x2A;
    static const uint8_t CMD_RASET    = 0x2B;
    static const uint8_t CMD_RAMWR    = 0x2C;
    static const uint8_t CMD_TEOFF    = 0x34;
    static const uint8_t CMD_TEON     = 0x35;
    static const uint8_t CMD_MADCTR   = 0x36;
    static const uint8_t CMD_COLMOD   = 0x3A;

    static const uint8_t COLMOD_12    = 3;
    static const uint8_t COLMOD_16    = 5;
    static const uint8_t COLMOD_18    = 6;

    static const uint8_t MADCTR_MY    = 0x80;
    static const uint8_t MADCTR_MX    = 0x40;
    static const uint8_t MADCTR_MV    = 0x20;
    static const uint8_t MADCTR_ML    = 0x10;   // Not implemented
    static const uint8_t MADCTR_RGB   = 0x08;   // Not implemented

    // Vendor-specific commands that we use to detect an LCD model
    static const uint8_t CMD_MAGIC_TRULY            = 0xC4;
    static const uint8_t CMD_MAGIC_TIANMA_HX8353    = 0xB9;
    static const uint8_t CMD_MAGIC_SANTEK_ST7735R   = 0xF6;
    static const uint8_t CMD_MAGIC_WnW_RM68116      = 0xF8;

    // Width of emulated TE pulses
    static const unsigned TE_WIDTH_US = 1000;

    uint32_t frame_count;
    uint32_t pixel_count;
    uint64_t te_timestamp;

    /* Hardware interface */
    uint8_t prev_wrx;

    /*
     * LCD Controller State
     */
 
    uint8_t current_cmd;
    uint8_t cmd_bytecount;
    uint8_t pixel_bytes[3];

    unsigned xs, xe, ys, ye;
    unsigned row, col;

    uint8_t madctr;
    uint8_t colmod;
    uint8_t mode_awake;
    uint8_t mode_display_on;
    uint8_t mode_te;
    uint8_t mode_power_on;

    /*
     * Model-specific emulation characteristics.
     *
     * These really just undo some of the model-specific tweaks
     * that the firmware performs. The emulation isn't particularly
     * accurate, but it's close enough for our purposes right now.
     */

    struct {
        uint8_t madctr_xor;
        int8_t row_adj;
        int8_t col_adj;
        uint8_t right_margin;
        uint8_t left_margin;
        uint8_t top_margin;
        uint8_t bottom_margin;
        enum {
            MIRROR_BEFORE_SWAP,
            SWAP_BEFORE_MIRROR,
        } order;
    } model;
};


};  // namespace Cube

#endif
