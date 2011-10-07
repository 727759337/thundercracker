/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo prototype simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * Emulate a nRF24L01/nRF24LE1 radio.
 *
 * We only handle ShockBurst PRX mode with auto-ack and a single
 * receive pipe (P0).
 */

#ifndef _RADIO_H
#define _RADIO_H

#include "radio.h"
#include "network.h"
#include "emulator.h"

class Radio {
 public:
    // Network backing this radio
    NetworkClient network;

    void init(struct em8051 *cpu) {
        memset(this, 0, sizeof *this);
        cpu = cpu;

        regs[REG_CONFIG] = 0x08;
        regs[REG_EN_AA] = 0x3F;
        regs[REG_EN_RXADDR] = 0x03;
        regs[REG_SETUP_AW] = 0x03;
        regs[REG_SETUP_RETR] = 0x03;
        regs[REG_RF_CH] = 0x02;
        regs[REG_RF_SETUP] = 0x0E;
        regs[REG_STATUS] = 0x0E;
        regs[REG_RX_ADDR_P0] = 0xE7;
        regs[REG_RX_ADDR_P1] = 0xC2;
        regs[REG_RX_ADDR_P2] = 0xC3;
        regs[REG_RX_ADDR_P3] = 0xC4;
        regs[REG_RX_ADDR_P4] = 0xC5;
        regs[REG_RX_ADDR_P5] = 0xC6;
        regs[REG_TX_ADDR] = 0xE7;
        regs[REG_FIFO_STATUS] = 0x11;

        memset(addr_tx_high, 0xE7, 4);
        memset(addr_rx0_high, 0xE7, 4);
        memset(addr_rx1_high, 0xC2, 4);
    }

    uint8_t *getRegs() {
        return regs;
    }
    
    uint32_t getRXCount() {
        uint32_t c = rx_count;
        rx_count = 0;
        return c;
    }

    uint32_t getByteCount() {
        uint32_t c = byte_count;
        byte_count = 0;
        return c;
    }

    uint8_t spiByte(uint8_t mosi) {
        // Chip not selected?
        if (!csn)
            return 0xFF;

        if (spi_index < 0) {
            spi_cmd = mosi;
            spiCmdBegin(mosi);
            spi_index++;
            return regs[REG_STATUS];
        }

        return spiCmdData(spi_cmd, spi_index++, mosi);
    }

    void radioCtrl(int csn, int ce) {
        if (csn && !csn) {
            // Begin new SPI command
            spi_index = -1;
        }
        
        if (!csn && csn) {
            // End an SPI command
            spiCmdEnd(spi_cmd);
        }
        
        csn = csn;
        ce = ce;
    }
    
    int tick() {
        /*
         * Simulate the rate at which we can actually receive RX packets
         * over the air, by giving ourselves a receive opportunity at
         * fixed clock cycle intervals.
         */
        if (ce && --rx_timer <= 0) {
            rx_timer = USEC_TO_CYCLES(RX_INTERVAL_US);
            rxOpportunity();
        }

        uint8_t irq = irq_edge;
        irq_edge = 0;
        return irq;
    }

 private:
    void updateIRQ() {
        uint8_t irq_prev = irq_state;
        uint8_t mask = (STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT) & ~regs[REG_CONFIG];

        irq_state = regs[REG_STATUS] & mask;
        irq_edge |= irq_state && !irq_prev;
    }
    
    void updateStatus() {
        regs[REG_FIFO_STATUS] =
            (rx_fifo_count == 0 ? FIFO_RX_EMPTY : 0) |
            (rx_fifo_count == FIFO_SIZE ? FIFO_RX_FULL : 0) |
            (tx_fifo_count == 0 ? FIFO_TX_EMPTY : 0) |
            (tx_fifo_count == FIFO_SIZE ? FIFO_TX_FULL : 0);

        regs[REG_STATUS] &= STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT;

        if (tx_fifo_count == FIFO_SIZE)
            regs[REG_STATUS] &= STATUS_TX_FULL;

        if (rx_fifo_count == 0)
            regs[REG_STATUS] |= STATUS_RX_P_MASK;

        regs[REG_RX_PW_P0] = rx_fifo[rx_fifo_tail].len;

        updateIRQ();
    }

    void rxOpportunity() {
        struct radio_packet *rx_head = &rx_fifo[rx_fifo_head];
        struct radio_packet *tx_tail = &tx_fifo[tx_fifo_tail];
        uint64_t src_addr;
        int len;
        uint8_t payload[256];    

        /* Network receive opportunity */
        network.setAddr(packAddr(REG_RX_ADDR_P0));
        len = network.rx(&src_addr, payload);
        if (len < 0 || len > (int) sizeof rx_head->payload)
            return;

        if (rx_fifo_count < FIFO_SIZE) {
            rx_head->len = len;
            memcpy(rx_head->payload, payload, len);

            rx_fifo_head = (rx_fifo_head + 1) % FIFO_SIZE;
            rx_fifo_count++;
            regs[REG_STATUS] |= STATUS_RX_DR;

            // Statistics for the debugger
            rx_count++;
            byte_count += len;

            if (tx_fifo_count) {
                // ACK with payload
                byte_count += tx_tail->len;
                network.tx(src_addr, tx_tail->payload, tx_tail->len);
                tx_fifo_tail = (tx_fifo_tail + 1) % FIFO_SIZE;
                tx_fifo_count--;
                regs[REG_STATUS] |= STATUS_TX_DS;
            } else {
                // ACK without payload (empty TX fifo)
                network.tx(src_addr, NULL, 0);
            }
        } else
            cpu->except(cpu, EXCEPTION_RADIO_XRUN);

        updateStatus();
    }
    
    uint8_t spiCmdData(uint8_t cmd, unsigned index, uint8_t mosi) {
        switch (cmd) {

        case CMD_R_RX_PAYLOAD:
            return rx_fifo[rx_fifo_tail].payload[index % PAYLOAD_MAX];

        case CMD_W_TX_PAYLOAD:
        case CMD_W_TX_PAYLOAD_NO_ACK:
        case CMD_W_ACK_PAYLOAD:
            tx_fifo[tx_fifo_head].payload[index % PAYLOAD_MAX] = mosi;
            return 0xFF;

        case CMD_W_REGISTER | REG_STATUS:
            // Status has write-1-to-clear bits
            mosi &= STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT;
            regRef(cmd, index) &= ~mosi;
            updateIRQ();
            return 0xFF;

        case CMD_R_RX_PL_WID:
            return rx_fifo[rx_fifo_tail].len;

        default:
            if (cmd < CMD_R_REGISTER + sizeof regs)
                return regRef(cmd, index);
            if (cmd < CMD_W_REGISTER + sizeof regs) {
                regRef(cmd, index) = mosi;
                return 0xFF;
            }
        }
        return 0xFF;
    }

    void spiCmdBegin(uint8_t cmd) {
        /*
         * Handle commands that have an immediate side-effect, even before
         * we get data bytes.
         */
        switch (cmd) {

        case CMD_FLUSH_TX:
            tx_fifo_head = 0;
            tx_fifo_tail = 0;
            tx_fifo_count = 0;
            updateStatus();
            break;

        case CMD_FLUSH_RX:
            rx_fifo_head = 0;
            rx_fifo_tail = 0;
            rx_fifo_count = 0;
            updateStatus();
            break;
        }
    }

    void spiCmdEnd(uint8_t cmd) {
        /*
         * End a command, invoked at the point where CS is deasserted.
         */
        switch (cmd) {

        case CMD_W_TX_PAYLOAD:
        case CMD_W_TX_PAYLOAD_NO_ACK:
        case CMD_W_ACK_PAYLOAD:
            tx_fifo[tx_fifo_head].len = spi_index;
            if (tx_fifo_count < FIFO_SIZE) {
                tx_fifo_count++;
                tx_fifo_head = (tx_fifo_head + 1) % FIFO_SIZE;
            } else
                cpu->except(cpu, EXCEPTION_RADIO_XRUN);
            break;
            
        case CMD_R_RX_PAYLOAD:
            if (rx_fifo_count > 0) {
                rx_fifo_count--;
                rx_fifo_tail = (rx_fifo_tail + 1) % FIFO_SIZE;
            } else
                cpu->except(cpu, EXCEPTION_RADIO_XRUN);
            break;
        }
    }

    uint8_t &regRef(uint8_t reg, unsigned byte_index) {
        reg &= sizeof regs - 1;
        
        if (byte_index > 4)
            byte_index = 4;

        if (byte_index > 0)
            switch (reg) {
            case REG_TX_ADDR:
                return addr_tx_high[byte_index - 1];
            case REG_RX_ADDR_P0:
                return addr_rx0_high[byte_index - 1];
            case REG_RX_ADDR_P1:
                return addr_rx1_high[byte_index - 1];
            }
        
        return regs[reg];
    }

    uint64_t packAddr(uint8_t reg) {
        /*
         * Encode the nRF24L01 packet address plus channel in the
         * 64-bit address word used by our network message hub.
         */

        uint64_t addr = 0;
        int i;

        for (i = 4; i >= 0; i--)
            addr = (addr << 8) | regRef(reg, i);

        return addr | ((uint64_t)regs[REG_RF_CH] << 56);
    }

    static const unsigned RX_INTERVAL_US = 440;

    /* SPI Commands */
    static const uint8_t CMD_R_REGISTER          = 0x00;
    static const uint8_t CMD_W_REGISTER          = 0x20;
    static const uint8_t CMD_R_RX_PAYLOAD        = 0x61;
    static const uint8_t CMD_W_TX_PAYLOAD        = 0xA0;
    static const uint8_t CMD_FLUSH_TX            = 0xE1;
    static const uint8_t CMD_FLUSH_RX            = 0xE2;
    static const uint8_t CMD_REUSE_TX_PL         = 0xE3;
    static const uint8_t CMD_R_RX_PL_WID         = 0x60;
    static const uint8_t CMD_W_ACK_PAYLOAD       = 0xA8;
    static const uint8_t CMD_W_TX_PAYLOAD_NO_ACK = 0xB0;
    static const uint8_t CMD_NOP                 = 0xFF;

    /* Registers */
    static const uint8_t REG_CONFIG              = 0x00;
    static const uint8_t REG_EN_AA               = 0x01;
    static const uint8_t REG_EN_RXADDR           = 0x02;
    static const uint8_t REG_SETUP_AW            = 0x03;
    static const uint8_t REG_SETUP_RETR          = 0x04;
    static const uint8_t REG_RF_CH               = 0x05;
    static const uint8_t REG_RF_SETUP            = 0x06;
    static const uint8_t REG_STATUS              = 0x07;
    static const uint8_t REG_OBSERVE_TX          = 0x08;
    static const uint8_t REG_RPD                 = 0x09;
    static const uint8_t REG_RX_ADDR_P0          = 0x0A;
    static const uint8_t REG_RX_ADDR_P1          = 0x0B;
    static const uint8_t REG_RX_ADDR_P2          = 0x0C;
    static const uint8_t REG_RX_ADDR_P3          = 0x0D;
    static const uint8_t REG_RX_ADDR_P4          = 0x0E;
    static const uint8_t REG_RX_ADDR_P5          = 0x0F;
    static const uint8_t REG_TX_ADDR             = 0x10;
    static const uint8_t REG_RX_PW_P0            = 0x11;
    static const uint8_t REG_RX_PW_P1            = 0x12;
    static const uint8_t REG_RX_PW_P2            = 0x13;
    static const uint8_t REG_RX_PW_P3            = 0x14;
    static const uint8_t REG_RX_PW_P4            = 0x15;
    static const uint8_t REG_RX_PW_P5            = 0x16;
    static const uint8_t REG_FIFO_STATUS         = 0x17;
    static const uint8_t REG_DYNPD               = 0x1C;
    static const uint8_t REG_FEATURE             = 0x1D;

    /* STATUS bits */
    static const uint8_t STATUS_TX_FULL          = 0x01;
    static const uint8_t STATUS_RX_P_MASK        = 0X0E;
    static const uint8_t STATUS_MAX_RT           = 0x10;
    static const uint8_t STATUS_TX_DS            = 0x20;
    static const uint8_t STATUS_RX_DR            = 0x40;

    /* FIFO_STATUS bits */
    static const uint8_t FIFO_RX_EMPTY           = 0x01;
    static const uint8_t FIFO_RX_FULL            = 0x02;
    static const uint8_t FIFO_TX_EMPTY           = 0x10;
    static const uint8_t FIFO_TX_FULL            = 0x20;
    static const uint8_t FIFO_TX_REUSE           = 0x40;

    static const uint8_t FIFO_SIZE   = 3;
    static const uint8_t PAYLOAD_MAX = 32;
    
    struct radio_packet {
        uint8_t len;
        uint8_t payload[PAYLOAD_MAX];
    };

    int rx_timer;
    uint32_t byte_count;
    uint32_t rx_count;
    uint8_t irq_state;
    uint8_t irq_edge;

    /*
     * Keep these consecutive, for the debugger's memory editor view.
     * It can currently view/edit 0x38 bytes, starting at regs[0].
     */
    union {
        uint8_t debug[0x38];
        struct {
            uint8_t regs[0x20];         // 00 - 1F
            uint8_t addr_tx_high[4];    // 20 - 23
            uint8_t addr_rx0_high[4];   // 24 - 27
            uint8_t addr_rx1_high[4];   // 28 - 2B
            uint8_t rx_fifo_count;      // 2C
            uint8_t tx_fifo_count;      // 2D
            uint8_t rx_fifo_head;       // 2E
            uint8_t rx_fifo_tail;       // 2F
            uint8_t tx_fifo_head;       // 30
            uint8_t tx_fifo_tail;       // 31
            uint8_t csn;                // 32
            uint8_t ce;                 // 33
            uint8_t spi_cmd;            // 34
            int8_t spi_index;           // 35
        };
    };

    radio_packet rx_fifo[FIFO_SIZE];
    radio_packet tx_fifo[FIFO_SIZE];

    struct em8051 *cpu; // Only for exception reporting!
};

#endif
