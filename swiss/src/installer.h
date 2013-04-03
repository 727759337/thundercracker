#ifndef INSTALLER_H
#define INSTALLER_H

#include "iodevice.h"
#include "usbvolumemanager.h"

#include <string>

class Installer
{
public:
    Installer(IODevice &_dev);

    static int run(int argc, char **argv, IODevice &_dev);

    int install(const char *path, int vid, int pid, bool launcher, bool forceLauncher, bool rpc);

private:
    int sendHeader(uint32_t filesz);
    bool getPackageMetadata(const char *path);
    unsigned getInstallableElfSize(FILE *f);
    bool sendFileContents(FILE *f, uint32_t filesz);
    bool commit();

    IODevice &dev;
    std::string package, version;
    bool isLauncher;
    bool isRPC;
};

#endif // INSTALLER_H
