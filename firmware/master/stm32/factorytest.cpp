#include "factorytest.h"
#include "usart.h"
#include "board.h"
#include "macros.h"

#include "radio.h"
#include "flash.h"

uint8_t FactoryTest::commandBuf[MAX_COMMAND_LEN];
uint8_t FactoryTest::commandLen;

/*
 * Table of test handlers.
 * Order must match the Command enum.
 */
FactoryTest::TestHandler const FactoryTest::handlers[] = {
    nrfCommsHandler,
    flashCommsHandler,
    flashReadWriteHandler,
    ledHandler,
    uniqueIdHandler
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
        if (commandLen >= MAX_COMMAND_LEN)
            commandLen = 0;

        commandBuf[commandLen++] = rxbyte;

        if (commandBuf[LEN_INDEX] == commandLen) {
            // dispatch to the appropriate handler
            uint8_t cmd = commandBuf[CMD_INDEX];
            if (cmd < arraysize(handlers)) {
                TestHandler handler = handlers[cmd];
                // arg[0] is always the command byte
                handler(commandLen - 1, commandBuf + 1);
            }

            commandLen = 0;
        }
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
void FactoryTest::nrfCommsHandler(uint8_t argc, uint8_t *args)
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
void FactoryTest::flashCommsHandler(uint8_t argc, uint8_t *args)
{
    Flash::JedecID id;
    Flash::readId(&id);

    uint8_t result = (id.manufacturerID == Flash::MACRONIX_MFGR_ID) ? 1 : 0;

    const uint8_t response[] = { 3, args[0], result };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * len: 4
 * args[1] - sector number
 * args[2] - offset into sector
 */
void FactoryTest::flashReadWriteHandler(uint8_t argc, uint8_t *args)
{
    uint32_t sectorAddr = args[1] * Flash::SECTOR_SIZE;
    uint32_t addr = sectorAddr + args[2];

    Flash::eraseSector(sectorAddr);

    const uint8_t txbuf[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    Flash::write(addr, txbuf, sizeof txbuf);

    // write/erase only wait before starting the *next* operation, so make sure
    // this write is complete before reading
    while (Flash::writeInProgress())
        ;

    uint8_t rxbuf[sizeof txbuf];
    Flash::read(addr, rxbuf, sizeof rxbuf);

    uint8_t result = (memcmp(txbuf, rxbuf, sizeof txbuf) == 0) ? 1 : 0;

    const uint8_t response[] = { 3, args[0], result };
    Usart::Dbg.write(response, sizeof response);
}

/*
 * args[1] - color, bit0 == green, bit1 == red
 */
void FactoryTest::ledHandler(uint8_t argc, uint8_t *args)
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
void FactoryTest::uniqueIdHandler(uint8_t argc, uint8_t *args)
{
    uint8_t response[2 + Board::UniqueIdNumBytes] = { sizeof(response), args[0] };
    memcpy(response + 2, Board::UniqueId, Board::UniqueIdNumBytes);
    Usart::Dbg.write(response, sizeof response);
}

IRQ_HANDLER ISR_USART3()
{
    FactoryTest::onUartIsr();
}
