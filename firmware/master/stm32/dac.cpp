/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "dac.h"

void Dac::init()
{
    RCC.APB1ENR |= (1 << 29); // enable dac peripheral clock
}

// channels: 1 or 2
void Dac::configureChannel(int ch, Trigger trig, Waveform waveform, uint8_t mask_amp, BufferMode buffmode)
{
    uint16_t reg =  (mask_amp << 8) |
                    (waveform << 6) |
                    (trig == TrigNone ? 0 : (trig << 3) | (1 << 2)) | // if not none, both enable the trigger and configure it
                    (buffmode << 1);
    DAC.CR |= (reg << ((ch - 1) * 16));
}

void Dac::enableChannel(int ch)
{
    DAC.CR |= (1 << ((ch - 1) * 16));
}

void Dac::disableChannel(int ch)
{
    DAC.CR &= ~(1 << ((ch - 1) * 16));
}

void Dac::enableDMA(int ch)
{
    DAC.CR |= (0x1000 << ((ch - 1) * 16));
}

void Dac::disableDMA(int ch)
{
    DAC.CR &= ~(0x1000 << ((ch - 1) * 16));
}    

uintptr_t Dac::address(int ch, DataFormat format)
{
    volatile DACChannel_t &dc = DAC.channels[ch - 1];
    return (uintptr_t) &dc.DHR[format];
}

void Dac::write(int ch, uint16_t data, DataFormat format)
{
    volatile DACChannel_t &dc = DAC.channels[ch - 1];
    dc.DHR[format] = data;
}

// TODO - this is only 8-bit dual, support other formats/alignments as needed
void Dac::writeDual(uint16_t data)
{
    DAC.DHR8RD = data;
}

