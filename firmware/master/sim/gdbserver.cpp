/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

// Must be before other headers
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define WINVER WindowsXP
#   define _WIN32_WINNT 0x502
#   include <windows.h>
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/select.h>
#   include <netinet/tcp.h>
#   include <netinet/in.h>
#   include <fcntl.h>
#   include <netdb.h>
#   include <unistd.h>
#   include <errno.h>
#   define closesocket(_s) close(_s)
#endif

#include "gdbserver.h"
#include "macros.h"
#include "elfdebuginfo.h"
#include "svmdebugpipe.h"
#include <string.h>
#include <stdio.h>
using namespace Svm;

#define LOG_PREFIX  "GDB Server: "

GDBServer GDBServer::instance;

void GDBServer::setDebugInfo(const ELFDebugInfo *info)
{
    instance.debugInfo = info;
}

void GDBServer::setMessageCallback(GDBServer::MessageCallback cb)
{
    instance.messageCb = cb;
}

void GDBServer::start(int port)
{
    /*
     * Spawn a thread which listens for connections from GDB
     */

    ASSERT(instance.running == false);
    ASSERT(instance.thread == NULL);

    instance.port = port;
    instance.running = true;
    instance.thread = new tthread::thread(threadEntry, (void*) &instance);
}

void GDBServer::stop()
{
    /*
     * Ask the background thread to stop at its next convenience,
     * asynchronously. Does not wait for the thread.
     */

    instance.running = false;
}

void GDBServer::threadEntry(void *param)
{
    GDBServer *self = (GDBServer *) param;
    self->threadMain();
}

void GDBServer::threadMain()
{
    /*
     * Thread for the GDB server. We start listening for incoming TCP
     * connections, and invoke handleClient() for each client connection.
     * Only one client can be connected at a time.
     */

    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_port = htons(port);

    int listenFD = socket(AF_INET, SOCK_STREAM, 0);

    unsigned long arg = 1;
    setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, (const char *)&arg, sizeof arg);

    if (bind(listenFD, (struct sockaddr *)&addr, sizeof addr) < 0) {
        fprintf(stderr, LOG_PREFIX "Can't bind to port!\n");
        return;
    }

    if (listen(listenFD, 1) < 0) {
        fprintf(stderr, LOG_PREFIX "Can't listen on socket\n");
        return;
    }
    
    fprintf(stderr, LOG_PREFIX "Listening on port %d. Connect with "
        "\"target remote localhost:%d\"\n", port, port);

    while (running) {
        struct sockaddr_in addr;
        socklen_t addrSize = sizeof addr;
        clientFD = accept(listenFD, (struct sockaddr *) &addr, &addrSize);
        if (clientFD < 0)
            break;

        fprintf(stderr, LOG_PREFIX "Client connected\n");
        handleClient();
        fprintf(stderr, LOG_PREFIX "Client disconnected\n");

        closesocket(clientFD);
    }
    
    closesocket(listenFD);
}

void GDBServer::handleClient()
{
    /*
     * This is the main per-client loop. Listen for bytes, which we send
     * to rxBytes() to be reassembled into GDB packets.
     */

    unsigned long arg;
    #ifdef SO_NOSIGPIPE
        arg = 1;
        setsockopt(clientFD, SOL_SOCKET, SO_NOSIGPIPE, &arg, sizeof arg);
    #endif
    arg = 1;
    setsockopt(clientFD, IPPROTO_TCP, TCP_NODELAY, (const char *)&arg, sizeof arg);

    resetPacketState();

    // On connect, GDB assumes we're already stopped
    debugBreak();

    while (1) {
        char rxBuffer[4096];
        int ret = recv(clientFD, rxBuffer, sizeof rxBuffer, 0);

        if (ret <= 0) {
            if (errno == EAGAIN)
                continue;
            if (ret < 0)
                fprintf(stderr, LOG_PREFIX "Read error (%d)\n", errno);
            return;
        }
        
        rxBytes(rxBuffer, ret);
        txFlush();
    }
}

void GDBServer::txByte(uint8_t byte)
{
    /*
     * Append one byte to the transmit buffer. If it's full, flush
     * it to the network stack.
     */

    if (txBufferLen == sizeof txBuffer)
        txFlush();
    txBuffer[txBufferLen++] = byte;
    txChecksum += byte;
}

void GDBServer::txFlush()
{
    /*
     * Send the contents of txBuffer, and empty it.
     */

    if (txBufferLen) {
        send(clientFD, txBuffer, txBufferLen, 0);
        txBufferLen = 0;
    }
}

void GDBServer::resetPacketState()
{
    /*
     * Reset any per-packet state. This is used when a new client is connected,
     * or when a packet is completed.
     */

    packetState = 0;
    txBufferLen = 0;
    rxPacketLen = 0;
    rxChecksum = 0;
}

void GDBServer::rxBytes(char *bytes, int len)
{
    /*
     * Gather raw received bytes into fully framed GDB packets, and dispatch
     * them to rxPacket(). This buffers partial packets in rxPacketBuf[].
     */
    
    enum {
        S_NOT_IN_PACKET = 0,
        S_PAYLOAD,
        S_CHECKSUM_0,
        S_CHECKSUM_1,
    };
    
    for (int i = 0; i < len; i++) {
        char byte = bytes[i];
        switch (packetState) {

        case S_NOT_IN_PACKET:
            if (byte == '$') {
                resetPacketState();
                packetState = S_PAYLOAD;
            } else if (byte == 0x03) {
                debugBreak();
                txByte('+');
                sendStopReasonReply();
            }
            break;

        case S_PAYLOAD:
            if (byte == '#') {
                // Begin checksum footer
                packetState = S_CHECKSUM_0;
            } else if (rxPacketLen < sizeof rxPacket - 1) {
                // Normal payload byte. Part of the checksum.
                rxChecksum += byte;
                rxPacket[rxPacketLen++] = byte;
            } else {
                // Overflowed receive buffer. Discard the packet.
                // Note the -1 above; we leave room for a NUL terminator.
                packetState = S_NOT_IN_PACKET;
                txByte('-');
            }
            break;

        case S_CHECKSUM_0:
            // First (most significant) checksum digit
            if ((rxChecksum >> 4) == digitFromHex(byte)) {
                packetState = S_CHECKSUM_1;
            } else {
                packetState = S_NOT_IN_PACKET;
                txByte('-');
            }
            break;

        case S_CHECKSUM_1:
            // Second (least significant) checksum digit
            packetState = S_NOT_IN_PACKET;
            if ((rxChecksum & 0xF) == digitFromHex(byte)) {
                // Successfully received
                txByte('+');
                
                // NUL terminate and dispatch.
                rxPacket[rxPacketLen] = 0;
                handlePacket();
            } else {
                txByte('-');
            }
            break;
        }
    }
}

char GDBServer::digitToHex(int i)
{
    static const char lut[] = "0123456789abcdef";
    return lut[i & 0xF];
}

int GDBServer::digitFromHex(char c)
{
    if (c < '0')    return -1;
    if (c <= '9')   return c - '0';
    if (c < 'A')    return -1;
    if (c <= 'F')   return c - 'A' + 10;
    if (c < 'a')    return -1;
    if (c <= 'f')   return c - 'a' + 10;
    return -1;
}

bool GDBServer::packetStartsWith(const char *str)
{
    return 0 == strncmp(rxPacket, str, strlen(str));
}

void GDBServer::txPacketBegin()
{
    txByte('$');
    txChecksum = 0;
}

void GDBServer::txPacketEnd()
{
    uint8_t sum = txChecksum;
    txByte('#');
    txHexByte(sum);
}

void GDBServer::txHexByte(uint8_t byte)
{
    txByte(digitToHex(byte >> 4));
    txByte(digitToHex(byte));
}

void GDBServer::txHexWord(uint32_t word)
{
    txHexByte(word >> 0);
    txHexByte(word >> 8);
    txHexByte(word >> 16);
    txHexByte(word >> 24);
}

void GDBServer::txString(const char *str)
{
    char c;
    while ((c = *str)) {
        txByte(c);
        str++;
    }
}

void GDBServer::txPacketString(const char *str)
{
    txPacketBegin();
    txString(str);
    txPacketEnd();
}

uint32_t GDBServer::message(uint32_t words)
{
    /*
     * Synchronously send a message and wait for a reply.
     * This uses a callback provided by our transport layer.
     */

    if (messageCb)
        return messageCb(msgCmd, words, msgReply);
    else
        return 0;
}

void GDBServer::handlePacket()
{
    switch (rxPacket[0])
    {
        case 'q': {
            if (packetStartsWith("qSupported")) return txPacketString("PacketSize=100");
            if (packetStartsWith("qOffsets"))   return txPacketString("Text=0;Data=0;Bss=0");
            break;
        }

        case '?': {
            // Why did we last stop?
            return sendStopReasonReply();
        }

        case 'g': {
            // Read all registers
            txPacketBegin();
            msgCmd[0] = Debugger::M_READ_REGISTERS | Debugger::ALL_REGISTER_BITS;
            replyToRegisterRead(Debugger::ALL_REGISTER_BITS, message(1));
            return txPacketEnd();
        }

        case 'G': {
            // Write all registers
            return txPacketEnd();
        }

        case 'm': {
            // Read memory
            int addr = 0, size = 0;
            txPacketBegin();
            if (sscanf(rxPacket, "m%x,%x", &addr, &size) == 2)
                while (size > 0) {
                    uint8_t buffer[128];
                    int chunk = MIN((int) sizeof buffer, size);
                    if (!readMemory(addr, buffer, chunk))
                        break;
                    for (int i = 0; i < chunk; ++i)
                        txHexByte(buffer[i]);
                    size -= chunk;
                }
            return txPacketEnd();
        }

        case 'M': {
            // Write memory
            return txPacketString("OK");
        }

        case 'c': {
            msgCmd[0] = Debugger::M_SIGNAL | Debugger::S_RUNNING;
            message(1);
            return;
        }

        case 's': {
            // Single step
            return;
        }
    }

    // Unsupported packet; empty reply
    txPacketString("");
}

void GDBServer::debugBreak()
{
    // Handle a ^C, stop the target
    msgCmd[0] = Debugger::M_SIGNAL | Debugger::S_INT;
    message(1);
}

void GDBServer::sendStopReasonReply()
{
    txPacketBegin();
    txByte('S');
    msgCmd[0] = Debugger::M_IS_STOPPED;
    if (message(1) >= 1)
        txHexByte(msgReply[0]);
    txPacketEnd();
}

bool GDBServer::readMemory(uint32_t addr, uint8_t *buffer, uint32_t bytes)
{
    if (debugInfo && debugInfo->readROM(addr, buffer, bytes))
        return true;

    // We have to go to hardware to complete this read. Break it up into
    // multiple reply-buffer-sized chunks.

    while (bytes) {
        uint32_t chunk = MIN(bytes, Debugger::MAX_REPLY_BYTES);
        if ((addr & Debugger::M_ARG_MASK) != addr)
            return false;

        msgCmd[0] = Debugger::M_READ_RAM | addr;
        msgCmd[1] = chunk;
        if (message(2) * sizeof(uint32_t) < chunk)
            return false;

        memcpy(buffer, msgReply, chunk);
        addr += chunk;
        buffer += chunk;
        bytes -= chunk;
    }

    return true;
}

void GDBServer::replyToRegisterRead(uint32_t bitmap, uint32_t replyLen)
{
    for (uint32_t r = 0; r < NUM_GDB_REGISTERS; r++) {
        uint32_t word = findRegisterInPacket(bitmap, regGDBtoSVM(r));
        txHexWord((word < replyLen) ? msgReply[word] : -1);
    }
}

uint32_t GDBServer::findRegisterInPacket(uint32_t bitmap, uint32_t svmReg)
{
    /*
     * In a register packet with the specified bitmap, where does the
     * register 'svmReg' occur? Returns -1 if the register is nowhere.
     */

    uint32_t word = 0;
    
    if (svmReg < 32)
        for (unsigned i = 0; i <= svmReg; i++)
            if (bitmap & Svm::Debugger::regBit(i)) {
                if (i == svmReg)
                    return word;
                else
                    word++;
            }

    return (uint32_t) -1;
}

uint32_t GDBServer::regGDBtoSVM(uint32_t r)
{
    /*
     * Convert GDB's ARM register numbers to SVM register numbers.
     */
    switch (r) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case REG_FP:
        case REG_SP:
        case REG_PC:
            return r;

        case 25:
            return REG_CPSR;

        default:
            return (uint32_t) -1;
    }
}
