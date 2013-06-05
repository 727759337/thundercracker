/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef NOR_SPI_H
#define NOR_SPI_H

#include "board.h"
#include "spi.h"
#include "flash_device.h"

class NorSpi
{
public:
    enum Status {
        Ok                  = 0,
        // from status register
        WriteInProgress     = (1 << 0),
        WriteEnableLatch    = (1 << 1),
        // from security register
        ProgramFail         = (1 << 5),
        EraseFail           = (1 << 6),
        WriteProtected      = (1 << 7)
    };

    NorSpi(GPIOPin _csn, SPIMaster _spi) :
        csn(_csn), spi(_spi)
    {}

#if defined(USE_MX25L128)
    static const uint8_t MFGR_ID = 0xC2;                //Macronix mfgr id
    static const unsigned CAPACITY = 1024*1024*16;      //(=128Mbit)
#elif defined(USE_MX25L256)
    static const uint8_t MFGR_ID = 0xC2;                //Macronix mfgr id
    static const unsigned CAPACITY = 1024*1024*32;      //(=256Mbit)
#elif defined(USE_W25Q256)
    static const uint8_t MFGR_ID = 0xEF;                //Winbond mfgr id
    static const unsigned CAPACITY = 1024*1024*32;      //(=256Mbit)
#else
    #error "flash device part not specified"
#endif
    void init();

    void read(uint32_t address, uint8_t *buf, unsigned len);
    void write(uint32_t address, const uint8_t *buf, unsigned len);
    void eraseBlock(uint32_t address);
    void chipErase();
    void fourByteMode(bool en);

    bool busy() {
        if (mightBeBusy) {
            mightBeBusy = readReg(ReadStatusReg) & WriteInProgress;
        }
        return mightBeBusy || dmaInProgress;
    }

    void readId(FlashDevice::JedecID *id);

    void deepSleep();
    void wakeFromDeepSleep();

    // should only be called from flash_device.cpp
    static void dmaCompletionCallback();

private:
    enum Command {
        Read                        = 0x03,
        Read4B                      = 0x13,
        FastRead                    = 0x0B,
        FastRead4B                  = 0x0C,
        Read2                       = 0xBB,
        Read4                       = 0xEB,
        ReadW4                      = 0xE7,
        PageProgram                 = 0x02,
        PageProgram4B               = 0x12,
        QuadPageProgram             = 0x38,
        SectorErase                 = 0x20,
        BlockErase32                = 0x52,
        BlockErase64                = 0xD8,
        ChipErase                   = 0xC7,
        WriteEnable                 = 0x06,
        WriteDisable                = 0x04,
        ReadStatusReg               = 0x05,
        ReadConfigReg               = 0x15,
        WriteStatusConfigReg        = 0x01,
        WriteProtectSelect          = 0x68,
        Enter4B                     = 0xB7,
        Exit4B                      = 0xE9,
        EnableQpi                   = 0x35,
        ResetQpi                    = 0xF5,
        SuspendProgramErase         = 0x30,
        DeepPowerDown               = 0xB9,
        ReleaseDeepPowerDown        = 0xAB,  // Also includes ReadElectronicID
        SetBurstLength              = 0xC0,
        ReadID                      = 0x9F,
        ReadMfrDeviceID             = 0x90,
        ReadUntilCSNHigh            = 0x5A,
        EnterSecureOtp              = 0xB1,
        ExitSecureOtp               = 0xC1,
        ReadSecurityReg             = 0x2B,
        WriteSecurityReg            = 0x2F,
        SingleBlockLock             = 0x36,
        SingleBlockUnlock           = 0x39,
        ReadBlockLock               = 0x3C,
        GangBlockLock               = 0x7E,
        GangBlockUnlock             = 0x98,
        Nop                         = 0x0,
        ResetEnable                 = 0x66,
        Reset                       = 0x99,
        ReleaseReadEnhanced         = 0xFF
    };

    GPIOPin csn;
    SPIMaster spi;
    bool mightBeBusy;

    ALWAYS_INLINE void spiBegin() {
        csn.setLow();
    }

    ALWAYS_INLINE void spiEnd() {
        csn.setHigh();
    }

    void ensureWriteEnabled();
    void waitWhileBusy();
    bool waitForDma();
    static volatile bool dmaInProgress;

    uint8_t readReg(Command cmd);
};

#endif // NOR_SPI_H
