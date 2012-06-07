/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "system.h"
#include "system_mc.h"
#include "svmmemory.h"
#include "flash_device.h"
#include "flash_storage.h"
#include "lua_filesystem.h"


void FlashDevice::read(uint32_t address, uint8_t *buf, unsigned len)
{
    FlashStorage::MasterRecord &storage = SystemMC::getSystem()->flash.data->master;

    if (address <= sizeof storage.bytes &&
        len <= sizeof storage.bytes &&
        address + len <= sizeof storage.bytes) {

        memcpy(buf, storage.bytes + address, len);
    } else {
        ASSERT(0 && "MC flash read() out of range");
    }

    LuaFilesystem::onRawRead(address, buf, len);
}

void FlashDevice::write(uint32_t address, const uint8_t *buf, unsigned len)
{
    FlashStorage::MasterRecord &storage = SystemMC::getSystem()->flash.data->master;

    if (address <= sizeof storage.bytes &&
        len <= sizeof storage.bytes &&
        address + len <= sizeof storage.bytes) {

        LuaFilesystem::onRawWrite(address, buf, len);

        // Program bits from 1 to 0 only.
        while (len) {
            storage.bytes[address] &= *buf;
            buf++;
            address++;
            len--;
        }

    } else {
        ASSERT(0 && "MC flash write() out of range");
    }
}

void FlashDevice::eraseSector(uint32_t address)
{
    FlashStorage::MasterRecord &storage = SystemMC::getSystem()->flash.data->master;

    if (address < FlashDevice::CAPACITY) {
        // Address can be anywhere inside the actual sector
        unsigned sector = address - (address % FlashDevice::SECTOR_SIZE);

        LuaFilesystem::onRawErase(address);

        memset(storage.bytes + sector, 0xFF, FlashDevice::SECTOR_SIZE);
        storage.eraseCounts[sector / FlashDevice::SECTOR_SIZE]++;

    } else {
        ASSERT(0 && "MC flash eraseSector() out of range");
    }
}

void FlashDevice::chipErase()
{
    FlashStorage::MasterRecord &storage = SystemMC::getSystem()->flash.data->master;
    memset(storage.bytes, 0xFF, sizeof storage.bytes);

    for (unsigned s = 0; s != arraysize(storage.eraseCounts); ++s)
        storage.eraseCounts[s]++;
}

void FlashDevice::init()
{
    // No-op in the simulator
}

bool FlashDevice::writeInProgress()
{
    // This is never async in the simulator
    return false;
}
