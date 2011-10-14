/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * Network support for interoperating with simulated master cube.
 * We have a very simple fully synchronous protocol with fixed-size
 * packets. This listens on a local socket that the master connects to.
 */

#ifndef _SYSTEM_NETWORK_H
#define _SYSTEM_NETWORK_H

#include "cube_radio.h"
#include "vtime.h"

class System;

class SystemNetwork {
 public:
    SystemNetwork()
        : listenFD(-1), clientFD(-1),
          packetIsWaiting(false) {}

    void init(const VirtualTime *vtime);
    void exit();

    ALWAYS_INLINE void tick(System &sys) {
        if (deadline.hasPassed())
            deadlineWork(sys);
    }
            
 private:
    static const unsigned POLL_MS = 100;
    static const uint16_t PORT = 2404;

    struct TXPacket {
        uint8_t ack;
        Cube::Radio::Packet p;
    };

    struct RXPacket {
        uint64_t tickDelta;
        uint64_t address;
        Cube::Radio::Packet p;
    };

    int listenFD;
    int clientFD;

    TickDeadline deadline;
    bool packetIsWaiting;
    SystemNetwork::RXPacket nextPacket;

    void setNonBlock(int fd);
    void tx(TXPacket &packet);
    bool rx(RXPacket &packet);
    void disconnect(int &fd);
    void deadlineWork(System &sys);
    void deliverPacket(System &sys, RXPacket &rx);
};

#endif
