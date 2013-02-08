/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2013 Sifteo, Inc. All rights reserved.
 */

/*
 * Driver for the Nordic nRF8001 Bluetooth Low Energy controller.
 */

#ifndef _NRF8001_H
#define _NRF8001_H

#include "spi.h"
#include "gpio.h"

class NRF8001 {
public:
    NRF8001(GPIOPin _reqn,
            GPIOPin _rdyn,
            SPIMaster _spi)
        : reqn(_reqn), rdyn(_rdyn), spi(_spi)
        {}

    static NRF8001 instance;

    void init();
    void isr();

private:
    struct ACICommandBuffer {
        uint8_t length;
        uint8_t command;
        uint8_t param[30];
    };

    struct ACIEventBuffer {
        uint8_t debug;
        uint8_t length;
        uint8_t event;
        uint8_t param[29];
    };

    GPIOPin reqn;
    GPIOPin rdyn;
    SPIMaster spi;

    // Owned by ISR context
    ACICommandBuffer txBuffer;
    ACIEventBuffer rxBuffer;
    bool requestsPending;
    uint8_t numSetupPacketsSent;
    uint8_t operatingMode;

    static void staticSpiCompletionHandler();
    void onSpiComplete();

    void requestTransaction();   // Ask nicely for produceCommand() to be called once.
    void produceCommand();       // Fill the txBuffer if we can. ISR context only.
    void handleEvent();          // Consume the rxBuffer. ISR context only.

    void produceSetupCommand();
};

#endif
