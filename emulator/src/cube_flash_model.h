/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
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

/*
 * This header holds all of the model-specific details for our flash
 * memory.  The intention is for flash.c to be as model-agnostic as
 * possible, so it's easier to switch to other NOR parts in the
 * future.
 *
 * Our current requirements:
 *
 *  - NOR flash, 4Mx8 (32 megabits)
 *
 *  - Low power (Many memories have an automatic standby feature, which gives them
 *    fairly low power usage when we're clocking them slowly. Our actual clock rate
 *    will be 2.67 MHz peak.)
 *
 *  - Fast program times. The faster we can get the better, this will directly
 *    impact asset download times.
 *
 *  - Fast erase times. This is very important- we'd like to stream assets in the
 *    background, but during an erase we can't refresh the screen at all until the
 *    erase finishes. This is clearly very bad for interactivity, so flashes with
 *    erase times in the tens of milliseconds are much better than flashes that
 *    take half a second or longer to erase.
 *
 * Our current design is based on the Winbond W29GL032C, which is what we emulate.
 * It supports a 32-byte (1/4 tile) write buffer, to speed up programming operations.
 */

#include <stdint.h>

#ifndef _CUBE_FLASH_MODEL_H
#define _CUBE_FLASH_MODEL_H

namespace Cube {


struct FlashModel {
    /*
     * Flash geometry
     */

    static const unsigned SIZE         = 4 * 1024 * 1024;
    static const unsigned SECTOR_SIZE  = 64 * 1024;
    static const unsigned BUFFER_SIZE  = 32;

    /*
     * Structure of the status byte, returned while the flash is busy.
     */
    
    static const uint8_t STATUS_DATA_INV      = 0x80;    // Inverted bit of the written data
    static const uint8_t STATUS_TOGGLE        = 0x40;    // Toggles during any busy state
    static const uint8_t STATUS_ERASE_TOGGLE  = 0x04;    // Toggles only during erase
    
    /*
     * Program/erase timing.
     *
     * Currently these are based on "typical" datasheet values. We may
     * want to test with maximum allowed values too.
     */

    static const unsigned PROGRAM_BYTE_TIME_US    = 6;
    static const unsigned PROGRAM_BUFFER_TIME_US  = 96;
    static const unsigned ERASE_SECTOR_TIME_US    = 18000;
    static const unsigned ERASE_CHIP_TIME_US      = 40000;
    
    /*
     * The subset of commands we emulate.
     *
     * Each command is expressed as a pattern which matches against a
     * buffer of the last CMD_LENGTH write cycles.
     */

    static const unsigned CMD_LENGTH = 6;

    struct command_sequence {
        uint16_t addr_mask;
        uint16_t addr;
        uint8_t data_mask;
        uint8_t data;
    };
    
    static const command_sequence cmd_byte_program[CMD_LENGTH];
    static const command_sequence cmd_sector_erase[CMD_LENGTH];
    static const command_sequence cmd_chip_erase[CMD_LENGTH];
    static const command_sequence cmd_buffer_begin[CMD_LENGTH];
};


};  // namespace Cube

#endif

