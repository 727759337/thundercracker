/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * Low level hardware setup for the STM32 board.
 */

#include <sifteo/abi.h>
#include "radio.h"
#include "runtime.h"
#include "hardware.h"

/* One function in the init_array segment */
typedef void (*initFunc_t)(void);

/* Addresses defined by our linker script */
extern "C" unsigned _stack;
extern "C" unsigned __bss_start;
extern "C" unsigned __bss_end;
extern "C" unsigned __data_start;
extern "C" unsigned __data_end;
extern "C" unsigned __data_src;
extern "C" initFunc_t __init_array_start;
extern "C" initFunc_t __init_array_end;


extern "C" void _start()
{
    /*
     * Set up clocks:
     *
     * XXX: No luck in running at 72 MHz at the moment.
     *      Is it the supply voltage/current? Running
     *      at 48 MHz for now.
     *
     *   - 8 MHz HSE (xtal) osc
     *   - PLL x6 => 48 MHz
     *   - SYSCLK at 48 MHz
     *   - HCLK at 48 MHz
     *   - APB1 at 12 MHz (/4)
     *       - SPI2 at 6 MHz
     *   - APB2 at 24 MHz (/2)
     *       - GPIOs
     *   - USB clock at 48 MHz (PLL /1)
     */

    RCC.CFGR = ( (1 << 22) |	// USBPRE (/1)
		 (4 << 18) |	// PLLMUL (x6)
		 (1 << 16) |	// PLLSRC (HSE)
		 (8 << 11) |    // PPRE2  (/2)
		 (5 << 8) |	// PPRE1  (/4)
		 (2 << 0) );	// SW (PLL)

    RCC.CR |= ( (1 << 16) |	// HSE ON
		(1 << 24) );	// PLL ON

    // Enable peripheral clocks
    RCC.APB1ENR = 0x00004000;	// SPI2
    RCC.APB2ENR = 0x0000003c;	// GPIOs

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
    
    /*
     * High-level hardware initialization
     */

    Radio::open();

    /*
     * Launch our game runtime!
     */

    Runtime::run();
}

extern "C" void *_sbrk(intptr_t increment)
{
    /*
     * The C standard library expects the OS to provide this function
     * for allocating more memory. We don't support this, always
     * return NULL.
     */
    return NULL;
}

__attribute__ ((section (".vectors"))) uintptr_t vector_table[] = {
    /*
     * Hardware vector table for reset, interrupts, stack, and
     * exceptions. We place this in the .vectors section, at
     * address zero.
     */
    (uintptr_t) &_stack,
    (uintptr_t) &_start,
};

