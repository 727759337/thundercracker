/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdint.h>
#include <protocol.h>
#include <cube_hardware.h>

#include "radio.h"
#include "sensors.h"
#include "sensors_i2c.h"
#include "sensors_nb.h"
#include "sensors_touch.h"

volatile uint8_t sensor_tick_counter;
volatile uint8_t sensor_tick_counter_high;

uint8_t nb_bits_remaining;
uint8_t nb_buffer[2];
uint8_t nb_tx_packet[2];

__bit nb_tx_mode;
__bit nb_rx_mask_state0;
__bit nb_rx_mask_state1;
__bit nb_rx_mask_state2;
__bit nb_rx_mask_bit0;
__bit touch;

#ifdef DEBUG_NBR
	uint8_t __idata nbr_data[4];
	uint8_t	nbr_temp;
	uint8_t __idata nbr_data_valid[2];
	uint8_t __idata nbr_data_invalid[2];
#endif

#ifdef DEBUG_TOUCH
    uint8_t touch_count;
#endif

#ifdef TOUCH_DEBOUNCE
    uint8_t touch_on;
    uint8_t touch_off;
#endif


static void i2c_tx(const __code uint8_t *buf)
{
    /*
     * Standalone I2C transmit, used during initialization.
     *
     * Transmits any number of transactions, which are sequences of bytes
     * that begin with a length byte. The sequence of transmissions
     * ends when a zero length is encountered.
     */

    uint8_t len;
    for (;;) {
        len = *(buf++);
        if (!len)
            return;

        do {
            uint8_t b = *(buf++);

            for (;;) {
                uint8_t status;

                // Transmit one byte, and wait for it.
                IR_SPI = 0;
                W2DAT = b;
                while (!IR_SPI);
            
                status = W2CON1;
                if (!(status & W2CON1_READY)) {
                    // Retry whole byte
                    continue;
                }
                if (status & W2CON1_NACK) {
                    // Failed!
                    W2CON0 |= W2CON0_STOP;
                    return;
                } else {
                    break;
                }
            }

        } while (--len);

        W2CON0 |= W2CON0_STOP;
    }
}


void sensors_init()
{
    /*
     * This is a maze of IRQs, all with very specific priority
     * assignments.  Here's a quick summary:
     *
     *  - Prio 0 (lowest)
     *
     *    Radio
     *
     *       This is a very long-running ISR. It really needs to be
     *       just above the main thread in priority, but no higher.
     *
     *  - Prio 1 
     *
     *    Accelerometer (I2C)
     *
     *       I2C is somewhat time-sensitive, since we do need to
     *       service it within a byte-period. But this is a fairly
     *       long-timescale operation compared to our other IRQs, and
     *       there's no requirement for predictable latency.
     *
     *  - Prio 2
     *
     *    Master clock (Timer 0)
     *
     *       We need to try our best to service Timer 0 with
     *       predictable latency, within several microseconds, so that
     *       we can reliably begin our neighbor transmit on-time. This
     *       is what keeps us inside the timeslot that we were
     *       assigned.
     *
     *       XXX: This isn't possible with Timer 0, since it shares a
     *       priority level with the radio! We could pick a different
     *       timebase for the master clock, but it's probably easier to
     *       thunk the RF IRQ onto a different IRQ that has a lower
     *       priority (in a different prio group). But let's wait to
     *       see if this is even a problem. So far in practice it doesn't
     *       seem to be.
     *
     *  - Prio 3 (highest)
     *
     *    Neighbor pulse counter (Timer 1)
     *
     *       Absolutely must have low and predictable latency. We use
     *       this timer to line up our bit clock with the received
     *       bit, an operation that needs to be accurate down to just
     *       a couple dozen clock cycles.
     *
     *    Neighbor bit TX/RX (Timer 2)
     *
     *       This is the timer we use to actually sample/generate bits.
     *       It's our baud clock. Timing is critical for the same reasons
     *       as above. (Luckily we should never have these two high-prio
     *       interrupts competing. They're mutually exclusive)
     */

    /*
     * A/D converter (MISC irq) Priority
     */

    IP1 |= 0x10;
    
    /*
     * I2C / 2Wire, for the accelerometer
     */

    MISC_PORT |= MISC_I2C;      // Drive the I2C lines high
    MISC_DIR &= ~MISC_I2C;      // Output drivers enabled

    W2CON0 = 0;                 // Reset I2C master
    W2CON0 = 1;                 // turn it on
    W2CON0 = 7;                 // Turn on I2C controller, Master mode, 100 kHz.
    INTEXP |= 0x04;             // Enable 2Wire IRQ -> iex3
    W2CON1 = 0;                 // Unmask interrupt, leave everything at default
    T2CON |= 0x40;              // iex3 rising edge
    IRCON = 0;                  // Reset interrupt flag (used below)
    
    // Put LIS3D in low power mode with all 3 axes enabled & block data update enabled
    {
        static const __code uint8_t init[] = {
            3, ACCEL_ADDR_TX, ACCEL_CTRL_REG1, ACCEL_REG1_INIT,
            3, ACCEL_ADDR_TX, ACCEL_CTRL_REG4, ACCEL_REG4_INIT,
            3, ACCEL_ADDR_TX, ACCEL_CTRL_REG6, ACCEL_IO_00,
            0
        };
        i2c_tx(init);
    }

    IRCON = 0;                  // Clear any spurious IRQs from the above initialization
    IP0 |= 0x04;                // Interrupt priority level 1.
    IEN_SPI_I2C = 1;            // Global enable for SPI interrupts

    /*
     * Timer 0: master sensor period
     *
     * This is set up as a 13-bit counter, fed by Cclk/12.
     * It overflows at 16000000 / 12 / (1<<13) = 162.76 Hz
     */

    TCON_TR0 = 1;               // Timer 0 running
    IEN_TF0 = 1;                // Enable Timer 0 interrupt

    /*
     * Timer 1: neighbor pulse counter
     *
     * We use Timer 1 as a pulse counter in the neighbor receiver.
     * When we're idle, waiting for a new neighbor packet, we set the
     * counter to 0xFFFF and get an interrupt when it overflows.
     * Subsequently, we check it on every bit-period to see if we
     * received any pulses during that bit's time window.
     *
     * By default, the timer will be stopped. We start it after
     * exiting transmit mode for the first time. (We only wait
     * for RX packets when we aren't transmitting)
     */     

    TMOD |= 0x50;               // Timer 1 is a 16-bit counter
    IP0 |= 0x08;                // Highest priority for TF1 interrupt
    IP1 |= 0x08;
    IEN_TF1 = 1;                // Enable TF1 interrupt

    /*
     * Timer 2: neighbor bit period timer
     *
     * This is a very fast timer that we use during active reception
     * or transmission of a neighbor packet. It accurately times out
     * bit periods that we use for generating pulses or for sampling
     * Timer 1.
     *
     * We need this to be on Timer 2, since at such a short period we
     * really get a lot of benefit from having automatic reload.
     *
     * This timer begins stopped. We start it on-demand when we need
     * to do a transmit or receive.
     */     

    CRCH = 0xff;                // Constant reload value
    CRCL = 0x100 - NB_BIT_TICKS;
    T2CON |= 0x10;              // Reload enabled
    TH2 = 0xff;                 // TL2 is set as needed, but TH2 is constant
    IP0 |= 0x20;                // Highest priority for TF2 interrupt
    IP1 |= 0x20;
    IEN_TF2_EXF2 = 1;           // Enable TF2 interrupt

    /*
     * XXX: Build a trivial outgoing neighbor packet based on our trivial cube ID.
     * Second byte is a complement of the first byte used for error-checking
     * Second byte is reordered so the last 3 bits are 0s and serve as damping bits
     *
     * The format of first byte is: "1 1 1 id[4] id[3] id[2] id[1] id[0]"
     * The format of second byte is: "/id[4] /id[3] /id[2] /id[1] /id[0] 0 0 0"
     *
     */

    nb_tx_packet[0] = 0xE0 | radio_get_cube_id();
    nb_tx_packet[1] = (~nb_tx_packet[0])<<3;

    /*
     * Initialize touch detection
     */
    #ifdef TOUCH_DEBOUNCE
        touch_on = TOUCH_DEBOUNCE_ON;
        touch_off = TOUCH_DEBOUNCE_OFF;
    #endif
}
