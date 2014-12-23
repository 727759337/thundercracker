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

#ifndef FACTORYTEST_H
#define FACTORYTEST_H

#include <stdint.h>
#include "usbprotocol.h"
#include "radio.h"

class FactoryTest
{
public:
    static const unsigned UART_MAX_COMMAND_LEN = 6;
    static const unsigned UART_LEN_INDEX = 0;   // first byte is length
    static const unsigned UART_CMD_INDEX = 1;   // second byte is opcode

    static void init();
    static void onUartIsr();
    static void usbHandler(const USBProtocolMsg &m);

    static void task();

    /*
     * RF test handlers.
     *
     * Since we're not sending meaningful protocol data, we treat any ACK,
     * with payload or not, as a success.
     *
     * Because the cube is toggling between 2 channels in its default
     * disconnected state, we send a packet on each, and count it as a
     * failure if we don't get an ACK from either one.
     */
    static void produce(PacketTransmission &tx);
    static void ALWAYS_INLINE ackWithPacket(const PacketBuffer &packet, unsigned retries) {
        if (rfTransmissionsRemaining) {
            rfSuccessCount++;
            rfTransmissionsRemaining--;
        }
    }
    static void ALWAYS_INLINE timeout() {
        if (rfTransmissionsRemaining) {
            rfTransmissionsRemaining--;
        }
    }
    static void ALWAYS_INLINE ackEmpty(unsigned retries) {
        if (rfTransmissionsRemaining) {
            rfSuccessCount++;
            rfTransmissionsRemaining--;
        }
    }

    static void onBtlePhaseComplete(uint8_t status, uint16_t response) {
        btleTest.status = status;
        btleTest.result = response;
        btleTest.inProgress = 0;
    }

private:

    struct UartCommand {
        uint8_t buf[UART_MAX_COMMAND_LEN];
        uint8_t len;

        ALWAYS_INLINE void append(uint8_t byte) {

            if (len >= UART_MAX_COMMAND_LEN) {
                len = 0; // avoid overflow - reset
            }
            buf[len++] = byte;
        }

        ALWAYS_INLINE bool complete() const {
            return buf[UART_LEN_INDEX] == len;
        }

        ALWAYS_INLINE uint8_t opcode() const {
            return buf[UART_CMD_INDEX];
        }
    };
    static UartCommand uartCommand;

    struct BtleTest {
        volatile uint8_t inProgress;
        uint8_t status;
        uint16_t result;
    };
    static BtleTest btleTest;

    static volatile uint16_t rfTransmissionsRemaining;
    static uint16_t rfSuccessCount;
    static RadioAddress rfTestAddr;
    static uint8_t rfTestAddrPrimaryChannel;
    static uint8_t rfTestCubeVersion;

    static const uint8_t RF_TEST_BYTE = 0x11;

    static void handleRfPacketComplete();

    typedef void(*TestHandler)(uint8_t argc, const uint8_t *args);
    static const TestHandler handlers[];

    static void nrfCommsHandler(uint8_t argc, const uint8_t *args);
    static void flashCommsHandler(uint8_t argc, const uint8_t *args);
    static void flashReadWriteHandler(uint8_t argc, const uint8_t *args);
    static void ledHandler(uint8_t argc, const uint8_t *args);
    static void uniqueIdHandler(uint8_t argc, const uint8_t *args);
    static void volumeCalibrationHandler(uint8_t argc, const uint8_t *args);
    static void batteryCalibrationHandler(uint8_t argc, const uint8_t *args);
    static void homeButtonHandler(uint8_t argc, const uint8_t *args);
    static void shutdownHandler(uint8_t argc, const uint8_t *args);
    static void audioTestHandler(uint8_t argc, const uint8_t *args);
    static void bootloadRequestHandler(uint8_t argc, const uint8_t *args);
    static void rfPacketTestHandler(uint8_t argc, const uint8_t *args);
    static void rebootRequestHandler(uint8_t argc, const uint8_t *args);
    static void getFirmwareVersion(uint8_t argc, const uint8_t *args);
    static void rtcTestHandler(uint8_t argc, const uint8_t *args);
    static void bleCommsHandler(uint8_t argc, const uint8_t *args);
};

#endif // FACTORYTEST_H
