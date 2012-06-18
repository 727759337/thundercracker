/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Driver for the Nordic nRF24L01 radio
 */

#ifndef _NRF24L01_H
#define _NRF24L01_H

#include "spi.h"
#include "gpio.h"
#include "radio.h"


class NRF24L01 {
public:
    NRF24L01(GPIOPin _ce,
             GPIOPin _irq,
             SPIMaster _spi)
        : irq(_irq), ce(_ce), spi(_spi),
          txBuffer(NULL, txData + 1), rxBuffer(rxData + 1), txnState(Idle)
          {}

    static NRF24L01 instance;

    void init();
    void beginTransmitting();

    void setTxPower(Radio::TxPower pwr);
    Radio::TxPower txPower();

    void setConstantCarrier(bool enabled, unsigned channel = 0);
    void setPRXMode(bool enabled);

    void isr();
    GPIOPin irq;

 private:
    enum Command {
        CMD_R_REGISTER          = 0x00,
        CMD_W_REGISTER          = 0x20,
        CMD_R_RX_PAYLOAD        = 0x61,
        CMD_W_TX_PAYLOAD        = 0xA0,
        CMD_FLUSH_TX            = 0xE1,
        CMD_FLUSH_RX            = 0xE2,
        CMD_REUSE_TX_PL         = 0xE3,
        CMD_R_RX_PL_WID         = 0x60,
        CMD_W_ACK_PAYLOAD       = 0xA8,
        CMD_W_TX_PAYLOAD_NO_ACK = 0xB0,
        CMD_NOP                 = 0xFF,
    };

    enum Register {
        REG_CONFIG              = 0x00,
        REG_EN_AA               = 0x01,
        REG_EN_RXADDR           = 0x02,
        REG_SETUP_AW            = 0x03,
        REG_SETUP_RETR          = 0x04,
        REG_RF_CH               = 0x05,
        REG_RF_SETUP            = 0x06,
        REG_STATUS              = 0x07,
        REG_OBSERVE_TX          = 0x08,
        REG_RPD                 = 0x09,
        REG_RX_ADDR_P0          = 0x0A,
        REG_RX_ADDR_P1          = 0x0B,
        REG_RX_ADDR_P2          = 0x0C,
        REG_RX_ADDR_P3          = 0x0D,
        REG_RX_ADDR_P4          = 0x0E,
        REG_RX_ADDR_P5          = 0x0F,
        REG_TX_ADDR             = 0x10,
        REG_RX_PW_P0            = 0x11,
        REG_RX_PW_P1            = 0x12,
        REG_RX_PW_P2            = 0x13,
        REG_RX_PW_P3            = 0x14,
        REG_RX_PW_P4            = 0x15,
        REG_RX_PW_P5            = 0x16,
        REG_FIFO_STATUS         = 0x17,
        REG_DYNPD               = 0x1C,
        REG_FEATURE             = 0x1D,
    };

    enum Status {
        RX_DR                   = 1 << 6,
        TX_DS                   = 1 << 5,
        MAX_RT                  = 1 << 4,
    };

    static const unsigned SOFT_RETRY_MAX = 32;

    GPIOPin ce;
    SPIMaster spi;

    PacketTransmission txBuffer;
    PacketBuffer rxBuffer;
    unsigned softRetriesLeft;

    /*
     * The extra byte here is required for the SPI command byte that must
     * precede the payload data.
     */
    uint8_t txData[PacketBuffer::MAX_LEN + 1];
    uint8_t rxData[PacketBuffer::MAX_LEN + 1];
    /*
     * NOTE: This exists because the RadioAddress struct does not provide room
     * for the extra byte we need to efficiently transmit these details via DMA.
     *
     * It's cheaper RAM-wise to maintain this single buffer than to extend
     * RadioAddress, which is stored in each Cube object, at the cost of a bit
     * of CPU to copy the data.
     */
    uint8_t txAddressBuffer[sizeof(RadioAddress::id) + 1];

    void handleTimeout();
    void receivePacket();
    void transmitPacket();
    void pulseCE();

    enum TransactionState {
        Idle,
        TXChannel,
        TXAddressTx,
        TXAddressRx,
        TXPayload,
        RXStatus,
        RXPayload
    };

    TransactionState txnState;

    static void staticSpiCompletionHandler(void *p);
    void onSpiComplete();
};

#endif
