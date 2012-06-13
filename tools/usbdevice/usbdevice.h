#ifndef _USB_DEVICE_H_
#define _USB_DEVICE_H_

#include "libusb.h"
#include <list>
#include <stdint.h>

class UsbDevice {
public:
    UsbDevice();

    static const unsigned MAX_EP_SIZE = 64;
    static const unsigned MAX_OUTSTANDING_OUT_TRANSFERS = 32;

    static int init() {
        return libusb_init(0);
    }

    static void processEvents() {
        struct timeval tv = {
            0,  // tv_sec
            0   // tv_usec
        };
        libusb_handle_events_timeout_completed(0, &tv, 0);
    }

    bool open(uint16_t vendorId, uint16_t productId, uint8_t interface = 0);
    void close();
    bool isOpen() const;

    int inEpMaxPacketSize() const {
        return mInEndpoint.maxPacketSize;
    }

    int outEpMaxPacketSize() const {
        return mOutEndpoint.maxPacketSize;
    }

    int numPendingINPackets() const {
        return mBufferedINPackets.size();
    }
    int readPacket(uint8_t *buf, unsigned maxlen);
    int readPacketSync(uint8_t *buf, int maxlen, int *transferred, unsigned timeout = -1);

    int numPendingOUTPackets() const {
        return mOutEndpoint.pendingTransfers.size();
    }
    int writePacket(const uint8_t *buf, unsigned len);
    int writePacketSync(const uint8_t *buf, int maxlen, int *transferred, unsigned timeout = -1);

private:
    /*
     * Work around what appears to be a libusb bug. Only the first 3
     * active transfers successfully cancel on shutdown, so to keep
     * things simple until this is fixed, only maintain 3 outstanding IN transfers.
     */
#ifdef WIN32
    static const unsigned NUM_CONCURRENT_IN_TRANSFERS = 3;
#else
    static const unsigned NUM_CONCURRENT_IN_TRANSFERS = 16;
#endif

    bool populateDeviceInfo(libusb_device *dev);
    void openINTransfers();

    static void LIBUSB_CALL onRxComplete(libusb_transfer *);
    static void LIBUSB_CALL onTxComplete(libusb_transfer *);

    void handleRx(libusb_transfer *t);
    void handleTx(libusb_transfer *t);

    struct Endpoint {
        uint8_t address;
        uint16_t maxPacketSize;
        std::list<libusb_transfer*> pendingTransfers;
    };

    Endpoint mInEndpoint;
    Endpoint mOutEndpoint;

    void removeTransfer(std::list<libusb_transfer*> &list, libusb_transfer *t);

    libusb_device_handle *mHandle;

    struct Packet {
        uint8_t *buf;
        uint8_t len;
    };
    std::list<Packet> mBufferedINPackets;
};

#endif // _USB_DEVICE_H_
