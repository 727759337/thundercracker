/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "spi.h"
#include "board.h"

void SPIMaster::init()
{
    if (hw == &SPI1) {
        #if BOARD == BOARD_TC_MASTER_REV1
        AFIO.MAPR |= (1 << 0);  // remap SPI1 to PB3-5
        #endif
        RCC.APB2ENR |= (1 << 12);
    }
    else if (hw == &SPI2) {
        RCC.APB1ENR |= (1 << 14);
    }
    else if (hw == &SPI3) {
        #if BOARD == BOARD_TC_MASTER_REV1
        AFIO.MAPR |= (1 << 28);     // remap SPI3 to PC10-12
        #endif
        RCC.APB1ENR |= (1 << 15);
    }

    csn.setHigh();
    csn.setControl(GPIOPin::OUT_10MHZ);
    sck.setControl(GPIOPin::OUT_ALT_10MHZ);
    miso.setControl(GPIOPin::IN_FLOAT);    
    mosi.setControl(GPIOPin::OUT_ALT_10MHZ);

    hw->CR1 = 0x0004;   // Master mode
    hw->CR2 = 0x0004;   // SS output enable
    hw->CR1 |= 0x0040;  // Peripheral enable
}

void SPIMaster::begin()
{
    csn.setLow();
}

void SPIMaster::end()
{
    csn.setHigh();
}

uint8_t SPIMaster::transfer(uint8_t b)
{
    /*
     * XXX: This is slow, ugly, and power hungry. We should be
     *      doing DMA, and keeping the FIFOs full! And NOT
     *      busy-looping ever!
     */
    hw->DR = b;
    while (!(hw->SR & 1));      // Wait for RX-not-empty
    return hw->DR;
}
    
void SPIMaster::transferTable(const uint8_t *table)
{
    /*
     * Table-driven transfers: Each is prefixed by a length byte.
     * Terminated by a zero-length transfer.
     */

    uint8_t len;
    while ((len = *table)) {
        table++;

        begin();
        do {
            transfer(*table);
            table++;
        } while (--len);
        end();
    }
}
