/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "usb/usbdevice.h"
#include "usb/usbcore.h"
#include "usb/usbhardware.h"
#include "usb/usbdefs.h"

#include "hardware.h"
#include "board.h"
#include "tasks.h"
#include "usbprotocol.h"
#include "macros.h"

#if (BOARD == BOARD_TEST_JIG)
#include "testjig.h"
#endif

static const Usb::DeviceDescriptor dev = {
    sizeof(Usb::DeviceDescriptor),  // bLength
    Usb::DescriptorDevice,          // bDescriptorType
    0x0200,                         // bcdUSB
    0xff,                           // bDeviceClass - 0xff == vendor specific
    0,                              // bDeviceSubClass
    0,                              // bDeviceProtocol
    64,                             // bMaxPacketSize0
    UsbDevice::VendorID,            // idVendor
    UsbDevice::ProductID,           // idProduct
    0x0200,                         // bcdDevice
    1,                              // iManufacturer
    2,                              // iProduct
    0,                              // iSerialNumber
    1                               // bNumConfigurations
};

/*
 * Configuration descriptor - the Usb::ConfigDescriptor must be first in the
 * struct, such that we can cast it correctly.
 */
static const struct {

    Usb::ConfigDescriptor       configDescriptor;
    Usb::InterfaceDescriptor    interfaceDescriptor;
    Usb::EndpointDescriptor     inEpDescriptor;
    Usb::EndpointDescriptor     outEpDescriptor;

} __attribute__((packed)) configurationBlock = {

    // configDescriptor
    {
        sizeof(Usb::ConfigDescriptor),      // bLength
        Usb::DescriptorConfiguration,       // bDescriptorType
        sizeof(configurationBlock),         // wTotalLength
        1,                                  // bNumInterfaces
        1,                                  // bConfigurationValue
        0,                                  // iConfiguration
        0x80,                               // bmAttributes
        0x32                                // bMaxPower
    },
    // interfaceDescriptor
    {
        sizeof(Usb::InterfaceDescriptor),   // bLength
        Usb::DescriptorInterface,           // bDescriptorType
        0,                                  // bInterfaceNumber
        0,                                  // bAlternateSetting
        2,                                  // bNumEndpoints
        0xFF,                               // bInterfaceClass
        0,                                  // bInterfaceSubClass
        0,                                  // bInterfaceProtocol
        0                                   // iInterface
    },
    // inEpDescriptor
    {
        sizeof(Usb::EndpointDescriptor),    // bLength
        Usb::DescriptorEndpoint,            // bDescriptorType
        UsbDevice::InEpAddr,                // bEndpointAddress
        Usb::EpAttrBulk,                    // bmAttributes
        64,                                 // wMaxPacketSize
        1,                                  // bInterval
    },
    // outEpDescriptor
    {
        sizeof(Usb::EndpointDescriptor),    // bLength
        Usb::DescriptorEndpoint,            // bDescriptorType
        UsbDevice::OutEpAddr,               // bEndpointAddress
        Usb::EpAttrBulk,                    // bmAttributes
        64,                                 // wMaxPacketSize
        1,                                  // bInterval
    }
};

static const char *descriptorStrings[] = {
    "x",
    "Sifteo Inc.",
#if (BOARD == BOARD_TEST_JIG)
    "Sifteo TestJig",
#else
    "Thundercracker",
#endif
};

/*
 * Windows specific descriptors.
 */
static const struct {

    Usb::WinUsbCompatIdHeaderDescriptor     header;
    Usb::WinUsbFunctionSectionDescriptor    functionSection;

} __attribute__((packed)) compatId = {
    // header
    {
       sizeof(compatId),    // dwLength
       0x0100,              // bcdVersion
       0x0004,              // wIndex
       1,                   // bCount
       { 0 },               // reserved0
    },
    // functionSection
    {
        0,                  // bFirstInterfaceNumber
        1,                  // reserved1
        "WINUSB",           // compatibleID
        { 0 },              // subCompatibleID
        { 0 }               // reserved2
    }
};

/*
    Called from within Tasks::work() to process usb OUT data on the
    main thread.
*/
void UsbDevice::handleOUTData(void *p)
{
    uint8_t buf[OutEpMaxPacket];
    int numBytes = UsbHardware::epReadPacket(OutEpAddr, buf, sizeof(buf));
    if (numBytes > 0) {
    // XXX: going to need to figure out what dispatch looks like here once
    // we get some actual protocol support in place
#if (BOARD == BOARD_TEST_JIG)
        switch (buf[0]) {

        case 0:
            USBProtocolHandler::onData(buf, numBytes);
            UsbDevice::write(buf, numBytes);
            break;

        case 1:
            TestJig::onTestDataReceived(buf + 1, numBytes - 1);
            break;
        }
#else
        USBProtocolHandler::onData(buf, numBytes);
#endif
    }
}

/*
    Called from within Tasks::work() to handle a usb IN event on the main thread.
    TBD whether this is actually helpful.
*/
void UsbDevice::handleINData(void *p) {
    (void)p;
}

void UsbDevice::init() {
    UsbCore::init(&dev, (Usb::ConfigDescriptor*)&configurationBlock, descriptorStrings);
}

/*
 * Our configuration has been set, we're now ready to enable the endpoints
 * for the selected configuration - we only ever have one for this device.
 */
void UsbDevice::onConfigComplete(uint16_t wValue)
{
    UsbHardware::epSetup(InEpAddr, Usb::EpAttrBulk, 64);
    UsbHardware::epSetup(OutEpAddr, Usb::EpAttrBulk, 64);
}

void UsbDevice::handleReset()
{

}

void UsbDevice::handleSuspend()
{

}

void UsbDevice::handleResume()
{

}

void UsbDevice::handleStartOfFrame()
{

}

/*
 * Called in ISR context - not taking action on this at the moment.
 */
void UsbDevice::inEndpointCallback(uint8_t ep)
{

}

/*
 * Called in ISR context. Flag the out task so that we can process the data
 * on the 'main' thread since we'll likely want to be doing some long
 * running things as a result - fetching from flash, etc.
 */
void UsbDevice::outEndpointCallback(uint8_t ep)
{
    Tasks::setPending(Tasks::UsbOUT, 0);
}

/*
 * Handle any specific control requests.
 * Handle Windows specific requests to auto-register ourself as a WinUSB device.
 */
int UsbDevice::controlRequest(Usb::SetupData *req, uint8_t **buf, uint16_t *len)
{
    if ((req->bmRequestType & Usb::ReqTypeVendor) == Usb::ReqTypeVendor) {
        if (req->bRequest == WINUSB_COMPATIBLE_ID) {

            if (req->wIndex == 0x04) {
                *len = MIN(*len, sizeof compatId);
                *buf = (uint8_t*)&compatId;
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Write a packet out - will block until the previous packet is done transmitting.
 * len can be greater than max packet size, but must be less than the available
 * space in the TX FIFO.
 */
int UsbDevice::write(const uint8_t *buf, unsigned len)
{
    while (UsbHardware::epTxInProgress(InEpAddr))
        ;
    return UsbHardware::epWritePacket(InEpAddr, buf, len);
}

int UsbDevice::read(uint8_t *buf, unsigned len)
{
    return UsbHardware::epReadPacket(OutEpAddr, buf, len);
}
