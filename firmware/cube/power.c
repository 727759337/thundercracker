/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "power.h"
#include "hardware.h"
#include "lcd.h"
#include "radio.h"

uint8_t power_sleep_timer;


void power_init(void)
{
    /*
     * If we're being powered on without a wakup-from-pin, go directly to
     * sleep. We don't wake up when the batteries are first inserted, only
     * when we get a touch signal.
     *
     * This default can be overridden by compiling with -DWAKE_ON_POWERUP.
     */

    /*
     * First, clean up after any possible sleep state we were just in. The
     * data sheet specifies that we need to rest PWRDWN to 0 after reading it,
     * and we need to re-open the I/O latch.
     */
    uint8_t powerupReason = PWRDWN;
    OPMCON = 0;
    TOUCH_WUPOC = 0;
    PWRDWN = 0;

#ifndef WAKE_ON_POWERUP
    if (!powerupReason)
        power_sleep();
#endif

    /*
     * Basic poweron
     */

    BUS_DIR = 0xFF;

    ADDR_PORT = 0;
    MISC_PORT = MISC_IDLE;
    CTRL_PORT = CTRL_IDLE;

    ADDR_DIR = 0;
    MISC_DIR = MISC_DIR_VALUE;
    CTRL_DIR = CTRL_DIR_VALUE;

    /*
     * Neighbor TX pins
     *
     * We enable pull-downs for input mode, when we're receiving pulses from
     * our neighbors. This improves the isolation between each side's input.
     *
     * High drive is enabled.
     */

    MISC_CON = 0x60;
    MISC_CON = 0x61;
    MISC_CON = 0x65;
    MISC_CON = 0x67;
}

void power_sleep(void)
{
#ifndef REV1
    /*
     * Turn off all peripherals, and put the CPU into Deep Sleep mode.
     * Order matters, don't cause bus contention.
     */

    /*
     * XXX: For the changes in Rev 2, we need to worry about sequencing
     *      3.3v and 2.0v supply shutdown. We should also sequence the
     *      2.0v shutdown and the flash WE/OE lines such that we can
     *      get WE/OE driven low while in deep sleep (to avoid back-powering
     *      the flash) without getting into any transient states where there
     *      is contention on the data bus.
     */

    lcd_sleep();                // Sleep sequence for LCD controller
    cli();                      // Stop all interrupt handlers
    radio_rx_disable();         // Take the radio out of RX mode

    RF_CKEN = 0;                // Stop the radio clock
    RNGCTL = 0;                 // RNG peripheral off
    ADCCON1 = 0;                // ADC peripheral off
    W2CON0 = 0;                 // I2C peripheral off
    S0CON = 0;                  // UART peripheral off
    SPIMCON0 = 0;               // External SPI disabled
    SPIRCON0 = 0;               // Radio SPI disabled

    BUS_DIR = 0xFF;             // Float the bus before we've set CTRL_PORT

    ADDR_PORT = 0;              // Address bus must be all zero
    MISC_PORT = MISC_IDLE;      // Neighbor hardware idle
    CTRL_PORT = CTRL_SLEEP;     // Turns off DC-DC converters

    ADDR_DIR = 0;               // Drive address bus
    MISC_DIR = 0xFF;            // All MISC pins as inputs (I2C bus pulled up)
    CTRL_DIR = CTRL_DIR_VALUE;  // All CTRL pins driven
    
    BUS_PORT = 0;               // Drive bus port low
    BUS_DIR = 0;

    /*
     * Wakeup on touch
     */

    TOUCH_WUPOC = TOUCH_WUOPC_BIT;

    // We must latch these port states, to preserve them during sleep.
    // This latch stays locked until early wakeup, in power_init().
    OPMCON = OPMCON_LATCH_LOCKED;

    while (1) {
        PWRDWN = 1;

        /*
         * We loop purely out of paranoia. This point should never be reached.
         * On wakeup, the system experiences a soft reset.
         */
    }
#endif
}
