/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "usart.h"
#include "gpio.h"
#include "board.h"

// NOTE - these divisors must reflect the startup values configured in setup.cpp
#define APB2RATE (72000000 / 2)
#define APB1RATE (72000000 / 4)

// static
Usart Usart::Dbg(&UART_DBG);

void Usart::init(GPIOPin rx, GPIOPin tx, int rate, StopBits bits)
{
    if (uart == &USART1) {
        RCC.APB2ENR |= (1 << 14);
    }
    else if (uart == &USART2) {
        RCC.APB1ENR |= (1 << 17);
    }
    else if (uart == &USART3) {
        RCC.APB1ENR |= (1 << 18);
    }
    else if (uart == &UART4) {
        RCC.APB1ENR |= (1 << 19);
    }
    else if (uart == &UART5) {
        RCC.APB1ENR |= (1 << 20);
    }

    rx.setControl(GPIOPin::IN_FLOAT);
    tx.setControl(GPIOPin::OUT_ALT_50MHZ);

    if (uart == &USART1)
        uart->BRR = APB2RATE / rate;
    else
        uart->BRR = APB1RATE / rate;

    uart->CR1 = (1 << 13) |     // UE - enable
                (0 << 12) |     // M - word length, 8 bits
                (0 << 8) |      // PEIE - interrupt on parity error
                (1 << 5) |      // RXNEIE - interrupt on ORE (overrun) or RXNE (rx register not empty)
                (1 << 3) |      // TE - transmitter enable
                (1 << 2);       // RE - receiver enable
    uart->CR2 = (1 << 14) |     // LINEN - LIN mode enable
                (bits << 12);   // STOP - bits

    uart->SR = 0;
    (void)uart->SR;  // SR reset step 1.
    (void)uart->DR;  // SR reset step 2.
}

void Usart::deinit()
{
    if (uart == &USART1) {
        RCC.APB2ENR &= ~(1 << 14);
    }
    else if (uart == &USART2) {
        RCC.APB1ENR &= ~(1 << 17);
    }
    else if (uart == &USART3) {
        RCC.APB1ENR &= ~(1 << 18);
    }
    else if (uart == &UART4) {
        RCC.APB1ENR &= ~(1 << 19);
    }
    else if (uart == &UART5) {
        RCC.APB1ENR &= ~(1 << 20);
    }
}

/*
 * Return the status register to indicate what kind of event we responded to.
 * If we received a byte and the caller provided a buf, give it to them.
 */
uint16_t Usart::isr(uint8_t *buf)
{
    uint16_t sr = uart->SR;
    uint8_t  dr = uart->DR;  // always read DR to reset SR

    // error states: ORE, NE, FE, PE
    if (sr & 0xf) {
        // do something helpful?
    }

    // RXNE: data available
    if (sr & STATUS_RXED) {
        if (buf) {
            *buf = dr;
        }
    }

    // TXE: transmission complete
    if (sr & STATUS_TXED) {

    }

    return sr;
}

/*
 * Just synchronous for now, to serve as a stupid data dump stream.
 */
void Usart::write(const uint8_t *buf, int size)
{
    while (size--)
        put(*buf++);
}

void Usart::write(const char *buf)
{
    while (*buf)
        put(*buf++);
}

void Usart::writeHex(uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    unsigned count = 8;

    while (count--) {
        put(digits[value >> 28]);
        value <<= 4;
    }
}

/*
 * Just synchronous for now, to serve as a stupid data dump stream.
 */
void Usart::read(uint8_t *buf, int size)
{
    while (size--) {
        while (!(uart->SR & (1 << 5))); // wait for data register to be not-not empty
        *buf++ = uart->DR;
    }
}

void Usart::put(char c)
{
    while (!(uart->SR & (1 << 7))); // wait for empty data register
    uart->DR = c;
}

char Usart::get()
{
    while (!(uart->SR & (1 << 5))); // wait for data register to be not-not empty
    return uart->DR;
}
