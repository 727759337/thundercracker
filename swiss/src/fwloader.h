#ifndef FW_LOADER_H
#define FW_LOADER_H

#include "iodevice.h"
#include <stdio.h>

class FwLoader
{
public:
    FwLoader(IODevice &_dev, bool rpc=false);

    static int run(int argc, char **argv, IODevice &dev);
    int requestBootloaderUpdate(unsigned int pid);
    int load(const char* path, unsigned int pid);

private:

    // bootloader versions that we know how to talk to
    static const unsigned VERSION_COMPAT_MIN = 1;
    static const unsigned VERSION_COMPAT_MAX = 1;

    bool bootloaderVersionIsCompatible(unsigned &swVersion, unsigned &hwVersion);
    void resetBootloader();
    bool checkFileDetails(FILE *f, uint32_t &plainsz, uint32_t &crc);
    bool sendFirmwareFile(FILE *f, uint32_t crc, uint32_t size);

    IODevice &dev;
    bool isRPC;
};

#endif // FW_LOADER_H
