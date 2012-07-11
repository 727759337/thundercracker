/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdint.h>
#include "battery.h"
#include "sensors.h"
#include "radio.h"

__bit battery_counter_toggle;


static void adc_busy_wait()
{
    /*
     * Wait for 'busy' bit to stabilize? Nordic's sample code does this,
     * but it isn't documented in the data sheet.
     */
     
    __asm
        mov  a, #5
        djnz acc, .
    __endasm ;
 
    // Wait for BUSY=0
    while (ADCCON1 & 0x40);
}


void battery_poll()
{
    /*
     * Poll the battery voltage infrequently, since it takes a nontrivial
     * amount of time and it must block flash, LCD, and touch. We poll
     * every time a particular bit in the high byte of the sensor timer
     * toggles. So, we can choose any power of two divisor of the sensor
     * clock as our battery polling rate.
     *
     * This is currently set up to poll at 1/1024 of the sensor clock, or
     * a period of 6.29 seconds. That corresponds to detecting toggles in
     * bit 2 of the high byte.
     *
     * The polarity here is inverted such that we'll poll immediately
     * after reset, instead of waiting for a whole period to elapse first.
     */

    if (sensor_tick_counter_high & (1<<2)) {
        if (!battery_counter_toggle)
            return;
        battery_counter_toggle = 0;
    } else {
        if (battery_counter_toggle)
            return;
        battery_counter_toggle = 1;
    }
    
    /*
     * Now we're committed to doing a battery measurement. The battery
     * voltage sense pin is shared with LCD write strobe pin, and it's
     * connected through a high series resistance- so we'll need a long
     * acquisition time.
     *
     * The code in lcd_end_frame is responsible for putting the LCD into
     * a state where additional write strobes will be harmless. So here,
     * we don't have to worry about clocking out extra pixels.
     */
     
    adc_busy_wait();
    
    ADDR_PORT = 0;      // Pre-discharge WR net capacitance
    ADDR_DIR = 1;       // Input mode
    
    ADCCON2 = 0x03;     // Maximum 36us acquisition window
    ADCCON3 = 0xc0;     // Left-justified, 12-bit
    
    // Start conversion, referenced to VDD
    ADCCON1 = 0x81 | (BATTERY_ADC_CH << 2);
    adc_busy_wait();

    /*
     * Atomically write the whole 16-bit value.
     * We don't bother diff'ing it, since there's
     * sure to be enough noise that the reading will
     * jitter around a little.
     */

    radio_irq_disable(); {
        __asm
            mov     (_ack_data + RF_ACK_BATTERY_V), _ADCDATL
            mov     (_ack_data + RF_ACK_BATTERY_V + 1), _ADCDATH
            orl     _ack_bits, #RF_ACK_BIT_BATTERY_V
        __endasm ;
    } radio_irq_enable();
    
    // Release ownership of the ADC
    ADDR_DIR = 0;
}
