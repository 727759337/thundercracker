/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "board.h"
#include "systime.h"
#include "tasks.h"
#include "usb/usbdevice.h"
#include "bootloader.h"
#include "powermanager.h"

/*
 * Bootloader specific entry point.
 * Lower level init happens in setup.cpp.
 */
void bootloadMain(bool userRequestedUpdate)
{
    PowerManager::init();

    /*
     * Nested Vectored Interrupt Controller setup.
     *
     * This won't actually enable any peripheral interrupts yet, since
     * those need to be unmasked by the peripheral's driver code.
     */

    NVIC.irqEnable(IVT.UsbOtg_FS);
    NVIC.irqPrioritize(IVT.UsbOtg_FS, 0x90);

    Bootloader loader;
    loader.init();
    loader.exec(userRequestedUpdate); // never returns
}
