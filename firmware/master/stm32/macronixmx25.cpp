/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "macronixmx25.h"
#include "flash_device.h"
#include "board.h"
#include "macros.h"


void MacronixMX25::init()
{
    GPIOPin writeProtect = FLASH_WP_GPIO;
    writeProtect.setControl(GPIOPin::OUT_2MHZ);
    writeProtect.setHigh();

    spi.init();

    // prepare to write the status register
    spi.begin();
    spi.transfer(WriteEnable);
    spi.end();

    spi.begin();
    spi.transfer(WriteStatusConfigReg);
    spi.transfer(0);    // ensure block protection is disabled
    spi.end();

    mightBeBusy = false;
    while (readReg(ReadStatusReg) != Ok) {
        ; // sanity checking?
    }
}

void MacronixMX25::read(uint32_t address, uint8_t *buf, unsigned len)
{
    const uint8_t cmd[] = { FastRead,
                            address >> 16,
                            address >> 8,
                            address >> 0,
                            Nop };  // dummy

    waitWhileBusy();
    spi.begin();

    spi.txDma(cmd, sizeof(cmd));
    waitForDma();

    spi.transferDma(buf, buf, len);
    waitForDma();

    spi.end();
}

/*
    Simple synchronous writing.
*/
void MacronixMX25::write(uint32_t address, const uint8_t *buf, unsigned len)
{
    while (len) {
        // align writes to PAGE_SIZE chunks
        uint32_t pagelen = FlashDevice::PAGE_SIZE - (address & (FlashDevice::PAGE_SIZE - 1));
        if (pagelen > len) pagelen = len;

        waitWhileBusy();
        ensureWriteEnabled();

        const uint8_t cmd[] = { PageProgram,
                                address >> 16,
                                address >> 8,
                                address >> 0 };
        spi.begin();
        spi.txDma(cmd, sizeof(cmd));

        len -= pagelen;
        address += pagelen;

        waitForDma();
        spi.txDma(buf, pagelen);
        buf += pagelen;
        waitForDma();

        spi.end();
        mightBeBusy = true;
    }
}

/*
 * Any address within a sector is valid to erase that sector.
 */
void MacronixMX25::eraseSector(uint32_t address)
{
    waitWhileBusy();
    ensureWriteEnabled();

    spi.begin();
    spi.transfer(SectorErase);
    spi.transfer(address >> 16);
    spi.transfer(address >> 8);
    spi.transfer(address >> 0);
    spi.end();

    mightBeBusy = true;
}

/*
 * Any address within a block is valid to erase that block.
 */
void MacronixMX25::eraseBlock(uint32_t address)
{
    waitWhileBusy();
    ensureWriteEnabled();

    spi.begin();
    spi.transfer(BlockErase64);
    spi.transfer(address >> 16);
    spi.transfer(address >> 8);
    spi.transfer(address >> 0);
    spi.end();

    mightBeBusy = true;
}

void MacronixMX25::chipErase()
{
    waitWhileBusy();
    ensureWriteEnabled();

    spi.begin();
    spi.transfer(ChipErase);
    spi.end();

    mightBeBusy = true;
}

void MacronixMX25::ensureWriteEnabled()
{
    do {
        spi.begin();
        spi.transfer(WriteEnable);
        spi.end();
    } while (!(readReg(ReadStatusReg) & WriteEnableLatch));
}

void MacronixMX25::readId(FlashDevice::JedecID *id)
{
    waitWhileBusy();    
    spi.begin();

    spi.transfer(ReadID);
    id->manufacturerID = spi.transfer(Nop);
    id->memoryType = spi.transfer(Nop);
    id->memoryDensity = spi.transfer(Nop);

    spi.end();
}

/*
 * Valid for any register that returns one byte of data in response
 * to a command byte. Only used internally for reading status and security
 * registers at the moment.
 */
uint8_t MacronixMX25::readReg(MacronixMX25::Command cmd)
{
    uint8_t reg;
    spi.begin();
    spi.transfer(cmd);
    reg = spi.transfer(Nop);
    spi.end();
    return reg;
}

void MacronixMX25::deepSleep()
{
    spi.begin();
    spi.transfer(DeepPowerDown);
    spi.end();
}

void MacronixMX25::wakeFromDeepSleep()
{
    spi.begin();
    spi.transfer(ReleaseDeepPowerDown);
    // 3 dummy bytes
    spi.transfer(Nop);
    spi.transfer(Nop);
    spi.transfer(Nop);
    // 1 byte old style electronic signature - could return this if we want to...
    spi.transfer(Nop);
    spi.end();
    // TODO - CSN must remain high for 30us on transition out of deep sleep
}

void MacronixMX25::waitWhileBusy()
{
    if (mightBeBusy) {
        while (readReg(ReadStatusReg) & WriteInProgress) {
            // Kill time.. not safe to execute tasks here.
        }
        mightBeBusy = false;
    }
}

void MacronixMX25::waitForDma()
{
    while (spi.dmaInProgress()) {
        // Kill time.. not safe to execute tasks here.
    }
}
