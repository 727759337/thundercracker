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

#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "hardware.h"

#ifdef DEBUG

uint8_t Debug::buffer[DEBUG_BUFFER_SIZE];
uint32_t Debug::bufferLen;


void Debug::dcc(uint32_t dccData)
{
    /*
     * Raw debug data for OpenOCD. Blocks indefinitely if OpenOCD
     * is not attached!
     */
       
     int bytes = 4;
     const uint32_t busy = 1;

     do {
         while (NVIC.DCRDR_l & busy);
         NVIC.DCRDR_l = ((dccData & 0xFF) << 8) | busy;
         dccData >>= 8;
     } while (--bytes);
}

void Debug::log(const char *str)
{
    /*
     * Low-level JTAG debug messages. Blocks indefinitely if
     * OpenOCD is not attached! To get these messages, run
     * "monitor target_request debugmsgs enable" in GDB.
     */

    const uint32_t command = 0x01;
    int len = 0;

    for (const char *p = str; *p && len < 0xFFFF; p++, len++);
    
    dcc(command | (len << 16));
    
    while (len > 0) {
        uint32_t data = ( str[0] 
                          | ((len > 1) ? (str[1] << 8) : 0)
                          | ((len > 2) ? (str[2] << 16) : 0)
                          | ((len > 3) ? (str[3] << 24) : 0) );
        dcc(data);
        len -= 4;
        str += 4;
    }
}

void Debug::logToBuffer(void const *data, uint32_t size)
{
    /*
     * Log some raw data to the buffer. It will get dumped en-masse to
     * OpenOCD only when the buffer is too full to accept the next
     * message, or when it's explicitly flushed.
     */

    if (size + bufferLen >= DEBUG_BUFFER_SIZE)
        dumpBuffer();

    buffer[bufferLen++] = size;
    memcpy(buffer + bufferLen, data, size);
    bufferLen += size;
}        

void Debug::dumpBuffer()
{
    static char line[DEBUG_BUFFER_MSG_SIZE * 4];
    uint8_t *p = buffer;
    uint8_t *end = buffer + bufferLen;

    while (p < end) {
        char *linePtr = line;
        uint8_t msgLen = *(p++);
        linePtr += sprintf(linePtr, "[%2d] ", msgLen);
        
        while (msgLen && p < end) {
            linePtr += sprintf(linePtr, "%02x", *(p++));
            msgLen--;
        }

        log(line);
    }
}

#endif  // DEBUG
