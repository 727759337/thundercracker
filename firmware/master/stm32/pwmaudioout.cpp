/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "pwmaudioout.h"
#include "audiomixer.h"
#include "tasks.h"
#include <stdio.h>
#include <string.h>

// uncomment to show sample rate on gpio pin
//#define SAMPLE_RATE_GPIO

#ifdef SAMPLE_RATE_GPIO
static GPIOPin tim4TestPin(&GPIOC, 4);
#endif


void PwmAudioOut::init(AudioMixer *mixer)
{
#ifdef SAMPLE_RATE_GPIO
    tim4TestPin.setControl(GPIOPin::OUT_50MHZ);
#endif

    this->mixer = mixer;
    buf.init();

    STATIC_ASSERT(AudioMixer::SAMPLE_HZ == 16000);
    sampleTimer.init(2200, 0);

    pwmTimer.init(PWM_FREQ, 0);
    pwmTimer.configureChannelAsOutput(pwmChan,
                                        HwTimer::ActiveHigh,
                                        HwTimer::Pwm1,
                                        HwTimer::ComplementaryOutput);

    // must default to non-differential state to avoid direct shorting
    suspend();
}

void PwmAudioOut::start()
{
    resume();
    sampleTimer.enableUpdateIsr();
    pwmTimer.enableChannel(pwmChan);
    pwmTimer.setDuty(pwmChan, PWM_FREQ / 2);    // 50% duty cycle == "off"
    Tasks::setPending(Tasks::AudioPull, &buf);
}

void PwmAudioOut::stop()
{
    sampleTimer.disableUpdateIsr();
    suspend(); // TODO - full shut down?
}

void PwmAudioOut::suspend()
{
    sampleTimer.disableUpdateIsr();
    pwmTimer.disableChannel(pwmChan);
    // ensure outputs are tied in the same direction to avoid leaking current
    // across the speaker
    outA.setControl(GPIOPin::OUT_2MHZ);
    outB.setControl(GPIOPin::OUT_2MHZ);
    outA.setHigh();
    outB.setHigh();
}

void PwmAudioOut::resume()
{
    outA.setControl(GPIOPin::OUT_ALT_50MHZ);
    outB.setControl(GPIOPin::OUT_ALT_50MHZ);
    pwmTimer.enableChannel(pwmChan);
    sampleTimer.enableUpdateIsr();
}

/*
 * Called when our sample rate timer's update ISR has fired.
 * Push the next sample into the output device.
 */
void PwmAudioOut::tmrIsr()
{
#ifdef SAMPLE_RATE_GPIO
    tim4TestPin.toggle();
#endif
    // DANGER DANGER DANGER!
    if (buf.readAvailable() < 2)
        return;

    uint16_t duty = buf.dequeue() + 0x8000;
    duty = (duty * pwmTimer.period()) / 0xFFFF; // scale to timer period
    pwmTimer.setDuty(pwmChan, duty);
}
