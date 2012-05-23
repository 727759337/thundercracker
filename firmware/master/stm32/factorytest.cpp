#include "factorytest.h"
#include "usart.h"
#include "board.h"
#include "macros.h"

#include "radio.h"
#include "flash_device.h"
#include "usb/usbdevice.h"
#include "usbprotocol.h"
#include "volume.h"
#include "homebutton.h"

uint8_t FactoryTest::commandBuf[FactoryTest::UART_MAX_COMMAND_LEN];
uint8_t FactoryTest::commandLen;

/*
 * Table of test handlers.
 * Order must match the Command enum.
 */
FactoryTest::TestHandler const FactoryTest::handlers[] = {
    nrfCommsHandler,            // 0
    flashCommsHandler,          // 1
    flashReadWriteHandler,      // 2
    ledHandler,                 // 3
    uniqueIdHandler,            // 4
    volumeCalibrationHandler,   // 5
    batteryCalibrationHandler,  // 6
    homeButtonHandler,          // 7
};

void FactoryTest::init()
{
    commandLen = 0;
}

/*
 * A new byte has arrived over the UART.
 * We're assembling a test command that looks like [len, opcode, params...]
 * If this byte overruns our buffer, reset our count of bytes received.
 * Otherwise, dispatch to the appropriate handler when a complete message
 * has been received.
 */
void FactoryTest::onUartIsr()
{
    uint8_t rxbyte;
    uint16_t status = Usart::Dbg.isr(&rxbyte);
    if (status & Usart::STATUS_RXED) {

        // avoid overflow - reset
        if (commandLen >= UART_MAX_COMMAND_LEN)
            commandLen = 0;

        commandBuf[commandLen++] = rxbyte;

        if (commandBuf[UART_LEN_INDEX] == commandLen) {
            // dispatch to the appropriate handler
            uint8_t cmd = commandBuf[UART_CMD_INDEX];
            if (cmd < arraysize(handlers)) {
                TestHandler handler = handlers[cmd];
                // arg[0] is always the command byte
                handler(commandLen - 1, commandBuf + 1);
            }

            commandLen = 0;
        }
    }
}

/*
 * Dispatch test commands via USB.
 * UsbMessages have a headers of UsbProtocol::HEADER_LEN bytes,
 * followed by a byte of test command, followed by payload data.
 */
void FactoryTest::usbHandler(const uint8_t *buf, unsigned len)
{
    uint8_t cmd = buf[USBProtocol::HEADER_LEN];
    if (cmd < arraysize(handlers)) {
        TestHandler handler = handlers[cmd];
        // arg[0] is always the 'command 'type' byte
        handler(len - USBProtocol::HEADER_LEN, buf + USBProtocol::HEADER_LEN);
    }
}

/**************************************************************************
 * T E S T  H A N D L E R S
 *
 * Each handler is passed a list of arguments, the first of which is always
 * the command opcode itself.
 *
 **************************************************************************/

/*
 * len: 3
 * args[1] - radio tx power
 */
void FactoryTest::nrfCommsHandler(uint8_t argc, const uint8_t *args)
{
    Radio::TxPower pwr = static_cast<Radio::TxPower>(args[1]);
    Radio::setTxPower(pwr);

    const uint8_t response[] = { 3, args[0], Radio::txPower() };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * len: 2
 * no args
 */
void FactoryTest::flashCommsHandler(uint8_t argc, const uint8_t *args)
{
    FlashDevice::JedecID id;
    FlashDevice::readId(&id);

    uint8_t result = (id.manufacturerID == FlashDevice::MACRONIX_MFGR_ID) ? 1 : 0;

    const uint8_t response[] = { 3, args[0], result };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * len: 4
 * args[1] - sector number
 * args[2] - offset into sector
 */
void FactoryTest::flashReadWriteHandler(uint8_t argc, const uint8_t *args)
{
    uint32_t sectorAddr = args[1] * FlashDevice::SECTOR_SIZE;
    uint32_t addr = sectorAddr + args[2];

    FlashDevice::eraseSector(sectorAddr);

    const uint8_t txbuf[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    FlashDevice::write(addr, txbuf, sizeof txbuf);

    // write/erase only wait before starting the *next* operation, so make sure
    // this write is complete before reading
    while (FlashDevice::writeInProgress())
        ;

    uint8_t rxbuf[sizeof txbuf];
    FlashDevice::read(addr, rxbuf, sizeof rxbuf);

    uint8_t result = (memcmp(txbuf, rxbuf, sizeof txbuf) == 0) ? 1 : 0;

    const uint8_t response[] = { 3, args[0], result };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * args[1] - color, bit0 == green, bit1 == red
 */
void FactoryTest::ledHandler(uint8_t argc, const uint8_t *args)
{
    GPIOPin green = LED_GREEN_GPIO;
    green.setControl(GPIOPin::OUT_2MHZ);

    GPIOPin red = LED_RED_GPIO;
    red.setControl(GPIOPin::OUT_2MHZ);

    uint8_t status = args[1];

    if (status & 1)
        green.setLow();
    else
        green.setHigh();

    if (status & (1 << 1))
        red.setLow();
    else
        red.setHigh();

    // no result - just respond to indicate that we're done
    const uint8_t response[] = { 2, args[0] };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * No args - just return hw id.
 */
void FactoryTest::uniqueIdHandler(uint8_t argc, const uint8_t *args)
{
    uint8_t response[2 + Board::UniqueIdNumBytes] = { sizeof(response), args[0] };
    memcpy(response + 2, Board::UniqueId, Board::UniqueIdNumBytes);
    Usart::Dbg.write(response, sizeof response);
}

/*
 * args[1]: position, 0 = low extreme, non-zero = high extreme
 *
 * response: test id, calibration state, 16-bit raw reading stored as calibration.
 */
void FactoryTest::volumeCalibrationHandler(uint8_t argc, const uint8_t *args)
{
    Volume::CalibrationState cs = args[1] ? Volume::CalibrationHigh : Volume::CalibrationLow;
    uint16_t rawValue = Volume::calibrate(cs);

    const uint8_t response[] = { args[0], args[1], rawValue & 0xff, (rawValue >> 8) & 0xff };
    UsbDevice::write(response, sizeof response);
}

/*
 *
 */
void FactoryTest::batteryCalibrationHandler(uint8_t argc, const uint8_t *args)
{

}

/*
 * no args - we just report the state of the home button.
 */
void FactoryTest::homeButtonHandler(uint8_t argc, const uint8_t *args)
{
    const uint8_t buttonState = HomeButton::isPressed() ? 1 : 0;

    const uint8_t response[] = { args[0], buttonState };
    UsbDevice::write(response, sizeof response);
}

IRQ_HANDLER ISR_USART3()
{
    FactoryTest::onUartIsr();
}
