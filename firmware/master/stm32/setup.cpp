/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Low level hardware setup for the STM32 board.
 */

#include "board.h"
#include "gpio.h"
#include "powermanager.h"
#include "bootloader.h"

#include <string.h>

/* One function in the init_array segment */
typedef void (*initFunc_t)(void);

/* Addresses defined by our linker script */
extern unsigned     __bss_start;
extern unsigned     __bss_end;
extern unsigned     __data_start;
extern unsigned     __data_end;
extern unsigned     __data_src;
extern initFunc_t   __init_array_start;
extern initFunc_t   __init_array_end;

extern void bootloadMain(bool)  __attribute__((noreturn));
extern int  main()              __attribute__((noreturn));

extern "C" void _start()
{
    /*
     * Don't need to run clock start up in application FW if the bootloader
     * has already run it.
     */
#ifndef BOOTLOADABLE
    /*
     * Set up clocks:
     *   - 8 MHz HSE (xtal) osc
     *   - PLL x9 => 72 MHz
     *   - SYSCLK at 72 MHz
     *   - HCLK at 72 MHz
     *   - APB1 at 18 MHz (/4)
     *       - SPI2 at 9 MHz
     *   - APB2 at 36 MHz (/2)
     *       - GPIOs
     *   - USB clock at 48 MHz (PLL /1)
     *
     * Other things that depend on our clock setup:
     *
     *   - SPI configuration. Keep nRF SPI as close to 10 MHz as we
     *     can without going over.
     *
     *   - SysTick frequency, in systime.cpp. The Cortex-M3's
     *     system clock is 1/8th the AHB clock.
     */

    // system runs from HSI on reset - make sure this is on and stable
    // before we switch away from it
    RCC.CR |= (1 << 0); // HSION
    while (!(RCC.CR & (1 << 1))); // wait for HSI ready

    RCC.CR &=  (0x1F << 3)  |   // HSITRIM reset value
               (1 << 0);        // HSION
    RCC.CFGR = 0;       // reset
    // Wait until HSI is the source.
    while ((RCC.CFGR & (3 << 2)) != 0x0);

    // fire up HSE
    RCC.CR |= (1 << 16); // HSEON
    while (!(RCC.CR & (1 << 17))); // wait for HSE to be stable

    // fire up the PLL
    RCC.CFGR |= (7 << 18) |                 // PLLMUL (x9)
                (RCC_CFGR_PLLXTPRE << 17) | // PLL XTPRE
                (1 << 16);                  // PLLSRC - HSE
    RCC.CR   |= (1 << 24);                  // turn PLL on
    while (!(RCC.CR & (1 << 25)));          // wait for PLL to be ready

    // configure all the other buses
    RCC.CFGR =  (0 << 24)                 | // MCO - mcu clock output
                (0 << 22)                 | // USBPRE - divide by 3
                (7 << 18)                 | // PLLMUL - x9
                (RCC_CFGR_PLLXTPRE << 17) | // PLL XTPRE
                (1 << 16)                 | // PLLSRC - HSE
                (2 << 14)                 | // ADCPRE - div6, ADCCLK is 14Mhz max
                (4 << 11)                 | // PPRE2 - APB2 prescaler, divide by 2
                (5 << 8)                  | // PPRE1 - APB1 prescaler, divide by 4
                (0 << 4);                   // HPRE - AHB prescaler, no divisor

    FLASH.ACR = (1 << 4) |  // prefetch buffer enable
                (1 << 1);   // two wait states since we're @ 72MHz

    // switch to PLL as system clock
    RCC.CFGR |= (2 << 0);
    while ((RCC.CFGR & (3 << 2)) != (2 << 2));   // wait till we're running from PLL

    // reset all peripherals
    RCC.APB1RSTR = 0xFFFFFFFF;
    RCC.APB1RSTR = 0;
    RCC.APB2RSTR = 0xFFFFFFFF;
    RCC.APB2RSTR = 0;

    // Enable peripheral clocks
    RCC.APB2ENR = 0x0000003d;    // GPIO/AFIO

#if 0
    // debug the clock output - MCO
    GPIOPin mco(&GPIOA, 8); // PA8 on the keil MCBSTM32E board - change as appropriate
    mco.setControl(GPIOPin::OUT_ALT_50MHZ);
#endif

    /*
     * Enable VCC SYS asap.
     */
    PowerManager::batteryPowerOn();

#endif // BOOTLOADABLE

    /*
     * Application firmware can request that the bootloader try to update by
     * writing BOOTLOAD_UPDATE_REQUEST_KEY to __data_start and
     * doing a system reset.
     *
     * Must check for this value before we overwrite it during normal init below.
     */
#ifdef BOOTLOADER
    bool updateRequested = (__data_start == Bootloader::UPDATE_REQUEST_KEY);
#endif

    /*
     * Initialize data segments (In parallel with oscillator startup)
     */

    memset(&__bss_start, 0, (uintptr_t)&__bss_end - (uintptr_t)&__bss_start);
    memcpy(&__data_start, &__data_src,
           (uintptr_t)&__data_end - (uintptr_t)&__data_start);

    /*
     * Run C++ global constructors.
     *
     * Best-practice for these is to keep them limited to only
     * initializing data: We shouldn't be talking to hardware, or
     * doing anything long-running here. Think of it as a way to
     * programmatically unpack data from flash to RAM, just like we
     * did above with the .data segment.
     */

    for (initFunc_t *p = &__init_array_start; p != &__init_array_end; p++)
        p[0]();

    // application specific entry point
#ifdef BOOTLOADER
    bootloadMain(updateRequested);
#else
    main();
#endif
}
