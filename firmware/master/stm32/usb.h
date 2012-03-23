/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>

class UsbDevice
{
public:
    static const uint16_t VendorID = 0x1234;
    static const uint16_t ProductID = 0x1234;

    static void init();

    static void handleINData(void *p);
    static void handleOUTData(void *p);

    static int read(uint8_t *buf, unsigned len);
    static int write(const uint8_t *buf, unsigned len);
};

#endif // USB_DEVICE_H
