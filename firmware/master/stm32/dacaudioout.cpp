/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "dacaudioout.h"
#include "audiomixer.h"
#include "tasks.h"
#include <stdio.h>
#include <string.h>

// uncomment to show sample rate on gpio pin
//#define SAMPLE_RATE_GPIO

#ifdef SAMPLE_RATE_GPIO
static GPIOPin tim4TestPin(&GPIOC, 4);
#endif

void DacAudioOut::init(AudioOutDevice::SampleRate samplerate, AudioMixer *mixer, GPIOPin &output)
{
#ifdef SAMPLE_RATE_GPIO
    tim4TestPin.setControl(GPIOPin::OUT_50MHZ);
#endif
    this->mixer = mixer;
    buf.init(&this->sys);
    // from datasheet: "once the DAC channel is aneabled, the corresponding gpio
    // is automatically connected to the analog converter output. to avoid
    // parasitic consumption, the pin should be configured to analog in"
    output.setControl(GPIOPin::IN_ANALOG);

    switch (samplerate) {
    case AudioOutDevice::kHz8000:  sampleTimer.init(2200, 0); break;
    case AudioOutDevice::kHz16000: sampleTimer.init(2200, 0); break;
    case AudioOutDevice::kHz32000: sampleTimer.init(550, 0); break;
    }

    dac.init();
    dac.configureChannel(dacChan); //, Waveform waveform = WaveNone, uint8_t mask_amp = 0, Trigger trig = TrigNone, BufferMode buffmode = BufferEnabled);

    Tasks::setPending(Tasks::AudioPull, &buf, true);
}

void DacAudioOut::start()
{
    sampleTimer.enableUpdateIsr();
    dac.enableChannel(this->dacChan);
}

void DacAudioOut::stop()
{
    sampleTimer.disableUpdateIsr();
    suspend(); // TODO - full shut down?
}

void DacAudioOut::suspend()
{
    dac.disableChannel(this->dacChan);
}

void DacAudioOut::resume()
{
    dac.enableChannel(this->dacChan);
}

#if 0
void DacAudioOut::dmaIsr(uint32_t flags)
{
    // figure out which of our buffers to pull into
    AudioOutBuffer *b = &audioBufs[0]; // TODO: ping pong
    b->remaining = mixer->pullAudio(b->data, sizeof(b->data));
    // kick off next DMA transfer
    if (!b->remaining) {
        suspend();
    }
}
#endif

/*
 * Called when our sample rate timer's update ISR has fired.
 * Push the next sample into the output device.
 */
void DacAudioOut::tmrIsr()
{
#ifdef SAMPLE_RATE_GPIO
    tim4TestPin.toggle();
#endif

    // DANGER DANGER DANGER
    if (buf.readAvailable() < 2)
        return;

    uint16_t duty = (buf.dequeue() | (buf.dequeue() << 8)) + 0x8000;
    duty = (duty * 0xFFF) / 0xFFFF; // scale to 12-bit DAC output
    dac.write(this->dacChan, duty, Dac::RightAlign12Bit);
}
