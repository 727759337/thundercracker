/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker firmware
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "dma.h"

/*
 * Several peripherals can be served by a single DMA channel, so this module
 * provides a centralized way to manage which peripherals are signed up
 * for particular DMA channels, and ensure only one is registered at a time.
 */

uint32_t Dma::Ch1Mask = 0;
uint32_t Dma::Ch2Mask = 0;
Dma::DmaHandler_t Dma::Ch1Handlers[7];
Dma::DmaHandler_t Dma::Ch2Handlers[5];

volatile DMAChannel_t * Dma::initChannel(volatile DMA_t *dma, int channel, DmaIsr_t func, void *param)
{
    if (dma == &DMA1) {

        if (Ch1Mask == 0) {
            RCC.AHBENR |= (1 << 0);
            dma->IFCR = 0x0FFFFFFF; // clear all ISRs
        } else if (Ch1Mask & (1 << channel)) {
            return 0;     // already registered :(
        }

        Ch1Mask |= (1 << channel); // mark it as enabled
        Ch1Handlers[channel].isrfunc = func;
        Ch1Handlers[channel].param = param;

    } else if (dma == &DMA2) {

        if (Ch2Mask == 0) {
            RCC.AHBENR |= (1 << 1);
            dma->IFCR = 0x0FFFFFFF; // clear all ISRs
        } else if (Ch2Mask & (1 << channel)) {
            return 0;     // already registered :(
        }

        Ch2Mask |= (1 << channel); // mark it as enabled
        Ch2Handlers[channel].isrfunc = func;
        Ch2Handlers[channel].param = param;
    }

    /*
     * Ensure this channel's ISR status and general configuration
     * registers are cleared.
     */
    dma->IFCR = 1 << (channel * 4);

    volatile DMAChannel_t *ch = &dma->channels[channel];
    ch->CNDTR = 0;
    ch->CCR = 0;

    return ch;
}

/*
    Disable the ISR for this channel & make it available for another registration.
    Turn off the entire DMA channel if no channels are enabled.
*/
void Dma::deinitChannel(volatile DMA_t *dma, int channel)
{
    if (dma == &DMA1) {

        Ch1Mask &= ~(1 << channel); // mark it as disabled
        if (Ch1Mask == 0) { // last one? shut it down
            RCC.AHBENR &= ~(1 << 0);
        }

        Ch1Handlers[channel].isrfunc = 0;
        Ch1Handlers[channel].param = 0;

    } else if (dma == &DMA2) {

        Ch2Mask &= ~(1 << channel); // mark it as disabled
        if (Ch2Mask == 0) { // last one? shut it down
            RCC.AHBENR &= ~(1 << 1);
        }

        Ch2Handlers[channel].isrfunc = 0;
        Ch2Handlers[channel].param = 0;
    }
}

/*
    Common dispatcher for all dma interrupts.
    Shift the flags for the given channel into the lowest 4 bits,
    clear the ISR status, and call back the handler.
*/
void Dma::serveIsr(volatile DMA_t *dma, int ch, DmaHandler_t &handler)
{
    uint32_t flags = (dma->ISR >> (ch * 4)) & 0xF;  // only bottom 4 bits are relevant to a single channel
    dma->IFCR = 1 << (ch * 4); // set the CGIFx bit to clear the whole channel
    if (handler.isrfunc) {
        handler.isrfunc(handler.param, flags);
    }
}

/*
 * DMA1
 */
IRQ_HANDLER ISR_DMA1_Channel1()
{
    Dma::serveIsr(&DMA1, 0, Dma::Ch1Handlers[0]);
}

IRQ_HANDLER ISR_DMA1_Channel2()
{
    Dma::serveIsr(&DMA1, 1, Dma::Ch1Handlers[1]);
}

IRQ_HANDLER ISR_DMA1_Channel3()
{
    Dma::serveIsr(&DMA1, 2, Dma::Ch1Handlers[2]);
}

IRQ_HANDLER ISR_DMA1_Channel4()
{
    Dma::serveIsr(&DMA1, 3, Dma::Ch1Handlers[3]);
}

IRQ_HANDLER ISR_DMA1_Channel5()
{
    Dma::serveIsr(&DMA1, 4, Dma::Ch1Handlers[4]);
}

IRQ_HANDLER ISR_DMA1_Channel6()
{
    Dma::serveIsr(&DMA1, 5, Dma::Ch1Handlers[5]);
}

IRQ_HANDLER ISR_DMA1_Channel7()
{
    Dma::serveIsr(&DMA1, 6, Dma::Ch1Handlers[6]);
}

/*
 * DMA2
 */

IRQ_HANDLER ISR_DMA2_Channel1()
{
    Dma::serveIsr(&DMA2, 0, Dma::Ch2Handlers[0]);
}

IRQ_HANDLER ISR_DMA2_Channel2()
{
    Dma::serveIsr(&DMA2, 1, Dma::Ch2Handlers[1]);
}

IRQ_HANDLER ISR_DMA2_Channel3()
{
    Dma::serveIsr(&DMA2, 2, Dma::Ch2Handlers[2]);
}

IRQ_HANDLER ISR_DMA2_Channel4()
{
    Dma::serveIsr(&DMA2, 3, Dma::Ch2Handlers[3]);
}

IRQ_HANDLER ISR_DMA2_Channel5() {
    Dma::serveIsr(&DMA2, 4, Dma::Ch2Handlers[4]);
}

