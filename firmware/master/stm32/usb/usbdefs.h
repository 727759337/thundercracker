#ifndef USB_DEFS_H
#define USB_DEFS_H

#include <stdint.h>

namespace Usb {

static inline uint8_t lowByte(uint16_t x) {
    return x & 0xff;
}

static inline uint8_t highByte(uint16_t  x) {
    return (x >> 8) & 0xff;
}

static inline bool isInEp(uint8_t addr) {
    return (addr & 0x80) != 0;
}

typedef void (*EpCallback)(uint8_t ep);
typedef void (*Callback)();

enum Transaction {
    TransactionIn,
    TransactionOut,
    TransactionSetup
};

// bmRequestType bit definitions
enum RequestType {
    ReqTypeDevice       = 0x00,
    ReqTypeInterface    = 0x01,
    ReqTypeEndpoint     = 0x02,
    ReqTypeClass        = 0x20,
    ReqTypeVendor       = 0x40,
    ReqTypeType         = 0x60,
    ReqTypeDirection    = 0x80
};

enum RequestVal {
    RequestGetStatus        = 0x0,
    RequestClearFeature     = 0x1,
    RequestSetFeature       = 0x3,
    RequestSetAddress       = 0x5,
    RequestGetDescriptor    = 0x6,
    RequestSetDescriptor    = 0x7,
    RequestGetConfiguration = 0x8,
    RequestSetConfiguration = 0x9,
    RequestGetInterface     = 0xa,
    RequestSetInterface     = 0xb,
    RequestSyncFrame        = 0xc
};

enum Feature {
    FeatureEndpointHalt = 0,
    FeatureDeviceRemoteWakeup = 1,
    FeatureTestMode = 2
};

enum EndpointType {
    Control     = 0,
    Isochronous = 1,
    Bulk        = 2,
    Interrupt   = 3,
    Mask        = 4
};

enum EndpointAttr {
    EpAttrControl = 0x00,
    EpAttrIsochronous = 0x01,
    EpAttrBulk = 0x02,
    EpAttrInterrupt = 0x03
};

enum DescriptorType {
    DescriptorDevice                    = 1,
    DescriptorConfiguration             = 2,
    DescriptorString                    = 3,
    DescriptorInterface                 = 4,
    DescriptorEndpoint                  = 5,
    DescriptorDeviceQualifier           = 6,
    DescriptorOtherSpeedConfiguration   = 7
};

enum DescriptorIndex {
    IndexLangIdString       = 0,
    IndexManufacturerString = 1,
    IndexProductString      = 2,
    IndexSerialNumString    = 3,
    IndexConfigString       = 4,
    IndexInterfaceString    = 5
};


struct SetupData {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));


struct Request {
    uint8_t   bmRequestType;
    uint8_t   bRequest;
    uint16_t  wValue;
    uint16_t  wIndex;
    uint16_t  wLength;
} __attribute__((packed));


struct DeviceDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed));


struct ConfigDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttibutes;
    uint8_t  MaxPower;
} __attribute__((packed));


struct InterfaceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));


struct EndpointDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed));


struct StringDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wstring[1];
} __attribute__((packed));


struct LanguageDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wlangid[1];
} __attribute__((packed));

} // namespace Usb

#endif // USB_DEFS_H
