/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdint.h>
#include "sensors.h"
#include "hardware.h"
#include "radio.h"
#include "time.h"

/*
 * These MEMSIC accelerometers are flaky little buggers! We're seeing
 * different chips ship with different addresses, so we have to do a
 * quick search if we don't get a response. Additionally, we're seeing
 * them deadlock the I2C bus occasionally. So, we try to work around
 * that by unjamming the bus manually before every poll.
 */

#define ADDR_SEARCH_START       0x20    // Start here...
#define ADDR_SEARCH_INC         0x02    // Add this
#define ADDR_SEARCH_MASK        0x2F    // And mask to this (Keep the 0x20 bit always)

uint8_t accel_addr = ADDR_SEARCH_START;
uint8_t accel_state;
uint8_t accel_x;

uint8_t neighbor_capture;


/*
 * I2C ISR --
 *
 *    This is where we handle accelerometer reads. Since I2C is a
 *    pretty slow bus, and the hardware has no real buffering to speak
 *    of, we use this IRQ to implement a state machine that executes
 *    our I2C read byte-by-byte.
 *
 *    These reads get kicked off during the Radio ISR, so that we'll
 *    start taking a new accel reading any time the previous reading,
 *    if any, had a chance to be retrieved.
 *
 *    The kick-off (writing the first byte) is handled by some inlined
 *    code in sensors.h, which we include in the radio ISR.
 */

void spi_i2c_isr(void) __interrupt(VECTOR_SPI_I2C) __naked
{
    __asm
        push    acc
        push    psw
        push    dpl
        push    dph
        push    _DPS
        mov     _DPS, #0

        ;--------------------------------------------------------------------
        ; Accelerometer State Machine
        ;--------------------------------------------------------------------

        ; Check status of I2C engine.

        mov     a, _W2CON1
        jnb     acc.0, as_ret           ; Not a real I2C interrupt. Ignore it.
        jb      acc.1, as_nack          ; Was not acknowledged!

        mov     dptr, #as_1
        mov     a, _accel_state
        jmp     @a+dptr

        ; 1. Address/Write finished, Send register address next

as_1:
        mov     _W2DAT, #0              
        mov     _accel_state, #(as_2 - as_1)
        sjmp    as_ret

        ; 2. Register address finished. Send repeated start, and address/read

as_2:
        orl     _W2CON0, #W2CON0_START  
        mov     a, _accel_addr
        orl     a, #1
        mov     _W2DAT, a
        mov     _accel_state, #(as_3 - as_1)
        sjmp    as_ret

        ; 3. Address/read finished. Subsequent bytes will be reads.

as_3:
        mov     _accel_state, #(as_4 - as_1)
        sjmp    as_ret

        ; 4. Read X axis. This is the second-to-last byte, so we queue a Stop condition.

as_4:
        mov     _accel_x, _W2DAT
        orl     _W2CON0, #W2CON0_STOP  
        mov     _accel_state, #(as_5 - as_1)
        sjmp    as_ret

        ; 5. Read Y axis. In rapid succession, store both axes and set the change flag
        ;    if necessary. This minimizes the chances of ever sending one old axis and
        ;    one new axis. In fact, since this interrupt is higher priority than the
        ;    RF interrupt, we are guaranteed to send synchronized updates of both axes.

as_5:

        mov     a, _W2DAT
        xrl     a, (_ack_data + RF_ACK_ACCEL + 0)
        jz      1$
        xrl     (_ack_data + RF_ACK_ACCEL + 0), a
        orl     _ack_len, #RF_ACK_LEN_ACCEL
1$:

        mov     a, _accel_x
        xrl     a, (_ack_data + RF_ACK_ACCEL + 1)
        jz      2$
        xrl     (_ack_data + RF_ACK_ACCEL + 1), a
        orl     _ack_len, #RF_ACK_LEN_ACCEL
2$:

        mov     _accel_state, #0
        sjmp    as_ret

        ; NACK handler. Stop, go back to the default state, try again.
        ;
        ; Also try a different I2C address... some of these accelerometer
        ; chips are shipping with different addresses!

as_nack:
        mov     a, _accel_addr
        add     a, #ADDR_SEARCH_INC
        anl     a, #ADDR_SEARCH_MASK
        mov     _accel_addr, a

        orl     _W2CON0, #W2CON0_STOP  
        mov     _accel_state, #0
        sjmp    as_ret

        ;--------------------------------------------------------------------

as_ret:
        pop     _DPS
        pop     dph
        pop     dpl
        pop     psw
        pop     acc
        reti
    __endasm ;
}


/*
 * Timer 0 ISR --
 *
 *    This is a relatively low-frequency ISR that we use to kick off
 *    reading most of our sensors. Currently it handles neighbor TX,
 *    and it initiates I2C reads.
 *
 *    This timer runs at 162.76 Hz. It's a convenient frequency for us
 *    to get from the 16 MHz oscillator, but it also works out nicely
 *    for other reasons:
 * 
 *      1. It's similar to our accelerometer's analog bandwidth
 *      2. It works out well as a neighbor transmit period (32x 192us slots)
 *      3. It's good for human-timescale interactivity
 *
 *    Because neighbor transmits must be interleaved, we allow the
 *    master to adjust the phase of Timer 0. This does not affect the
 *    period. To keep the latencies predictable, we perform our
 *    neighbor transmit first-thing in this ISR.
 */

void tf0_isr(void) __interrupt(VECTOR_TF0) __naked
{
    __asm

        ;--------------------------------------------------------------------
        ; Neighbor TX
        ;--------------------------------------------------------------------

        ; XXX fake

        orl     MISC_PORT, #MISC_NB_OUT
        anl     _MISC_DIR, #~MISC_NB_OUT
        nop
        nop
        nop
        nop
        orl     _MISC_DIR, #MISC_NB_OUT

        ;--------------------------------------------------------------------
        ; Accelerometer Sampling
        ;--------------------------------------------------------------------

        ; Trigger the next accelerometer read, and reset the I2C bus. We do
        ; this each time, to avoid a lockup condition that can persist
        ; for multiple packets. We include a brief GPIO frob first, to try
        ; and clear the lockup on the accelerometer end.

        orl     MISC_PORT, #MISC_I2C      ; Drive the I2C lines high
        anl     _MISC_DIR, #~MISC_I2C     ;   Output drivers enabled
        xrl     MISC_PORT, #MISC_I2C_SCL  ;   Now pulse SCL low
        mov     _accel_state, #0          ; Reset accelerometer state machine
        mov     _W2CON0, #0               ; Reset I2C master
        mov     _W2CON0, #1               ;   Turn on I2C controller
        mov     _W2CON0, #7               ;   Master mode, 100 kHz.
        mov     _W2CON1, #0               ;   Unmask interrupt
        mov     _W2DAT, _accel_addr       ; Trigger the next I2C transaction

        reti
    __endasm;
}


/*
 * Timer 1 ISR --
 *
 *    We use Timer 1 in counter mode, to count incoming neighbor pulses.
 *    We use it both to detect the start pulse, and to detect subsequent
 *    pulses in our serial stream.
 *
 *    We only use the Timer 1 ISR to detect the start pulse. We keep the
 *    counter loaded with 0xFFFF when the receiver is idle. At other times,
 *    the counter value is small and won't overflow.
 *
 *    This interrupt is at the highest priority level! It's very important
 *    that we detect the start pulse with predictable latency, so we can
 *    time the subsequent pulses accurately.
 */

void tf1_isr(void) __interrupt(VECTOR_TF1) __naked
{
    __asm
        inc     _neighbor_capture
        mov     TH1, #0xFF
        mov     TL1, #0xFF
        1$: sjmp 1$
        reti
    __endasm;
}


/*
 * Timer 2 ISR --
 *
 *    We use Timer 2 to measure out individual bit-periods, during
 *    active transmit/receive of a neighbor packet.
 */

void tf2_isr(void) __interrupt(VECTOR_TF2) __naked
{
    __asm
        reti
    __endasm;
}


void sensors_init()
{
    /*
     * I2C, for the accelerometer
     *
     * This MUST be a high priority interrupt. Specifically, it
     * must be higher priority than the RFIRQ. The RFIRQ can take
     * a while to run, and it'll cause an underrun on I2C. Since
     * there's only a one-byte buffer for I2C reads, this is fairly
     * time critical.
     *
     * Currently we're using priority level 2.
     */

    INTEXP |= 0x04;             // Enable 2Wire IRQ -> iex3
    T2CON |= 0x40;              // iex3 rising edge
    IRCON = 0;                  // Clear any spurious IRQs from initialization
    IP1 |= 0x04;                // Interrupt priority level 2.
    IEN_SPI_I2C = 1;            // Global enable for SPI interrupts

    /*
     * Timer0, for triggering sensor reads.
     *
     * This is set up as a 13-bit counter, fed by Cclk/12.
     * It overflows at 16000000 / 12 / (1<<13) = 162.76 Hz
     */

    TCON |= 0x10;               // Timer 0 running
    IEN_TF0 = 1;                // Enable Timer 0 interrupt

    /*
     * Neighbor pulse counter.
     *
     * We use Timer 1 as a pulse counter in the neighbor receiver.
     * When we're idle, waiting for a new neighbor packet, we set the
     * counter to 0xFFFF and get an interrupt when it overflows.
     * Subsequently, we check it on every bit-period to see if we
     * received any pulses during that bit's time window.
     */     

    TMOD = 0x50;                // Timer 1 is a 16-bit counter
    TL1 = 0xff;                 // Overflow on the next pulse
    TH1 = 0xff;
    IP0 |= 0x08;                // Highest priority for TF1 interrupt
    IP1 |= 0x08;
    TCON |= 0x40;               // Timer 1 running
    IEN_TF1 = 1;                // Enable IFP
}
