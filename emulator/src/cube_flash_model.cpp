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
 * Static data for flash_model.h
 */

#include "cube_flash_model.h"

namespace Cube {


const FlashModel::command_sequence FlashModel::cmd_byte_program[FlashModel::CMD_LENGTH] = {
    { 0x000, 0x000, 0x00, 0x00 },   // Dont' care
    { 0x000, 0x000, 0x00, 0x00 },   // Don't care
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   // Unlock
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0xA0 },   //   ...
    { 0x000, 0x000, 0x00, 0x00 },   // Data address/byte
};

const FlashModel::command_sequence FlashModel::cmd_sector_erase[FlashModel::CMD_LENGTH] = {
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   // Unlock
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0x80 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   //   ...
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0x000, 0x000, 0xFF, 0x30 },   // Erase address
};

const FlashModel::command_sequence FlashModel::cmd_chip_erase[FlashModel::CMD_LENGTH] = {
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   // Unlock
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0x80 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   //   ...
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0xFFF, 0xAAA, 0xFF, 0x10 },   // Confirm
};

const FlashModel::command_sequence FlashModel::cmd_buffer_begin[FlashModel::CMD_LENGTH] = {
    { 0x000, 0x000, 0x00, 0x00 },   // Dont' care
    { 0x000, 0x000, 0x00, 0x00 },   // Don't care
    { 0xFFF, 0xAAA, 0xFF, 0xAA },   // Unlock
    { 0xFFF, 0x555, 0xFF, 0x55 },   //   ...
    { 0x000, 0x000, 0xFF, 0x25 },   // Sector address + command
    { 0x000, 0x000, 0x00, 0x00 },   // Sector address + length
};


};  // namespace Cube
