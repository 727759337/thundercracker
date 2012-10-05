#include "delete.h"
#include "util.h"
#include "usbprotocol.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int Delete::run(int argc, char **argv, IODevice &_dev)
{
    if (!_dev.open(IODevice::SIFTEO_VID, IODevice::BASE_PID))
        return 1;

    Delete m(_dev);
    bool success = false;
    unsigned volCode;

    if (argc == 2 && !strcmp(argv[1], "--all")) {
        success = m.deleteEverything();

    } else if (argc == 2 && !strcmp(argv[1], "--reformat")) {
        success = m.deleteReformat();

    } else if (argc == 2 && !strcmp(argv[1], "--sys")) {
        success = m.deleteSysLFS();

    } else if (argc == 2 && Util::parseVolumeCode(argv[1], volCode)) {
        success = m.deleteVolume(volCode);

    } else {
        fprintf(stderr, "incorrect args\n");
    }

    return success ? 0 : 1;
}

Delete::Delete(IODevice &_dev) :
    dev(_dev)
{}

bool Delete::deleteEverything()
{
    USBProtocolMsg m(USBProtocol::Installer);
    m.header |= UsbVolumeManager::DeleteEverything;

    return dev.writeAndWaitForReply(m);
}

bool Delete::deleteReformat()
{
    USBProtocolMsg m(USBProtocol::Installer);
    m.header |= UsbVolumeManager::DeleteReformat;

    fprintf(stderr, "This will take a few minutes...\n");

    return dev.writeAndWaitForReply(m);
}

bool Delete::deleteSysLFS()
{
    USBProtocolMsg m(USBProtocol::Installer);
    m.header |= UsbVolumeManager::DeleteSysLFS;

    return dev.writeAndWaitForReply(m);
}

bool Delete::deleteVolume(unsigned code)
{
    USBProtocolMsg m(USBProtocol::Installer);
    m.header |= UsbVolumeManager::DeleteVolume;
    m.append((uint8_t*) &code, sizeof code);

    return dev.writeAndWaitForReply(m);

    dev.writePacket(m.bytes, m.len);
}
