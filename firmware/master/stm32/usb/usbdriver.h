#ifndef USBDRIVER_H
#define USBDRIVER_H

#include "usb/usbdefs.h"

/*
 * Platform specific interface to perform hardware related USB tasks.
 */
class UsbDriver
{
public:
    UsbDriver();

    static void init();
    static void setAddress(uint8_t addr);
    static void epSetup(uint8_t addr, uint8_t type, uint16_t max_size, Usb::EpCallback cb);
    static void epReset();
    static void epStallSet(uint8_t addr, uint8_t stall);
    static void epNakSet(uint8_t addr, uint8_t nak);
    static bool epIsStalled(uint8_t addr);
    static uint16_t epWritePacket(uint8_t addr, const void *buf, uint16_t len);
    static uint16_t epReadPacket(uint8_t addr, void *buf, uint16_t len);
    static void poll();
    static void disconnect(bool disconnected);
};

#endif // USBDRIVER_H
