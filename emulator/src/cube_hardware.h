/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _CUBE_HARDWARE_H
#define _CUBE_HARDWARE_H

#include "cube_cpu.h"
#include "cube_radio.h"
#include "cube_adc.h"
#include "cube_accel.h"
#include "cube_spi.h"
#include "cube_i2c.h"
#include "cube_mdu.h"
#include "cube_rng.h"
#include "cube_lcd.h"
#include "cube_flash.h"
#include "cube_neighbors.h"
#include "cube_cpu_core.h"
#include "cube_debug.h"
#include "vtime.h"

namespace Cube {

/*
 * This file simulates the hardware peripherals that we have directly
 * attached to the 8051:
 *
 *  - NOR Flash (Design supports up to 16 megabits, 21-bit addressing)
 *  - LCD Controller
 *
 * We have four 8-bit I/O ports:
 *
 *  P3.6     Flash OE
 *  P3.5     Flash WE
 *  P3.4     LCD Backlight / Reset
 *  P3.3     3.3v Enable
 *  P3.2     Latch A20-A14 from P1.7-1 on rising edge
 *  P3.1     Latch A13-A7 from P1.7-1 on rising edge
 *  P3.0     LCD DCX
 *
 *  P2.7-0   Shared data bus, Flash + LCD
 *
 *  P1.7     Neighbor Out 4
 *  P1.6     Neighbor Out 3
 *  P1.5     Neighbor Out 2
 *  P1.4     Neighbor In  (INT2)
 *  P1.3     Accelerometer SDA
 *  P1.2     Accelerometer SCL
 *  P1.1     Neighbor Out 1
 *  P1.0     Touch sense in (AIN8)
 *
 *  P0.7-1   Flash A6-A0
 *  P0.0     LCD WRX
 */

static const uint8_t ADDR_PORT       = REG_P0;
static const uint8_t MISC_PORT       = REG_P1;
static const uint8_t BUS_PORT        = REG_P2;
static const uint8_t CTRL_PORT       = REG_P3;

static const uint8_t ADDR_PORT_DIR   = REG_P0DIR;
static const uint8_t MISC_PORT_DIR   = REG_P1DIR;
static const uint8_t BUS_PORT_DIR    = REG_P2DIR;
static const uint8_t CTRL_PORT_DIR   = REG_P3DIR;

static const uint8_t CTRL_LCD_DCX    = (1 << 0);
static const uint8_t CTRL_FLASH_LAT1 = (1 << 1);
static const uint8_t CTRL_FLASH_LAT2 = (1 << 2);
static const uint8_t CTRL_FLASH_WE   = (1 << 5);
static const uint8_t CTRL_FLASH_OE   = (1 << 6);

// RFCON bits
static const uint8_t RFCON_RFCKEN    = 0x04;
static const uint8_t RFCON_RFCSN     = 0x02;
static const uint8_t RFCON_RFCE      = 0x01;


class Hardware {
 public:
    CPU::em8051 cpu;
    VirtualTime *time;
    LCD lcd;
    SPIBus spi;
    I2CBus i2c;
    ADC adc;
    MDU mdu;
    FlashStorage flashStorage;
    Flash flash;
    Neighbors neighbors;
    RNG rng;
    
    bool init(VirtualTime *masterTimer,
              const char *firmwareFile, const char *flashFile);
    void exit();
    
    void reset();

    ALWAYS_INLINE bool tick() {
        bool cpuTicked = CPU::em8051_tick(&cpu);
        hardwareTick();
        return cpuTicked;
    }

    void lcdPulseTE() {
        lcd.pulseTE(hwDeadline);
    }

    void setAcceleration(float xG, float yG);
    void setTouch(float amount);

    bool isDebugging();

    // SFR side-effects
    void sfrWrite(int reg);
    int sfrRead(int reg);
    void debugByte();
    void graphicsTick();
    
    ALWAYS_INLINE void setRadioClockEnable(bool e) {
        rfcken = e;
    }
    
    uint32_t getExceptionCount();
    void incExceptionCount();
    
 private:

    ALWAYS_INLINE void hardwareTick()
    {
        /*
         * A big chunk of our work can happen less often than every
         * clock cycle, as determined by a simple deadline tracker.
         *
         * We also trigger hardware writes via a simpler (and shorter
         * to inline) method, just by writing to a byte in the CPU
         * struct. This is used by the inline SFR callbacks
         * in cube_cpu_callbacks.h.
         */

        if (hwDeadline.hasPassed() || cpu.needHardwareTick)
            hwDeadlineWork();

        /*
         * A few small things currently have to happen per-tick
         */

        neighbors.tick(cpu);
        if (LIKELY(flash_drv))
            cpu.mSFR[BUS_PORT] = flash.dataOut();
    }

    void hwDeadlineWork();
    TickDeadline hwDeadline;

    uint8_t lat1;
    uint8_t lat2;
    uint8_t bus;
    uint8_t prev_ctrl_port;
    uint8_t flash_drv;
    uint8_t rfcken;
    
    uint32_t exceptionCount;
};

};  // namespace Cube

#endif
