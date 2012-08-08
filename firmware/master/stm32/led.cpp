/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "led.h"
#include "gpio.h"
#include "board.h"
#include "macros.h"
#include "hwtimer.h"

namespace LED {
    static LEDSequencer sequencer;
}

void LED::init()
{
    sequencer.init();

    const HwTimer pwmTimer(&LED_PWM_TIM);
    const HwTimer seqTimer(&LED_SEQUENCER_TIM);
    const GPIOPin outRed = LED_RED_GPIO;
    const GPIOPin outGreen = LED_GREEN_GPIO;

    /*
     * PWM: Full 16-bit resolution, about 1 kHz period
     *
     * NOTE: this timer is also used by BatteryLevel, since the battery
     * measurement pinout requires it. Currently, BatteryLevel gets
     * initialized after LED, but if you change that, you'll need to adjust
     * it there as well.
     */
    pwmTimer.init(0xFFFF, 0);
    pwmTimer.configureChannelAsOutput(LED_PWM_GREEN_CHAN,
        HwTimer::ActiveLow, HwTimer::Pwm1, HwTimer::SingleOutput);
    pwmTimer.configureChannelAsOutput(LED_PWM_RED_CHAN,
        HwTimer::ActiveLow, HwTimer::Pwm1, HwTimer::SingleOutput);

    pwmTimer.enableChannel(LED_PWM_GREEN_CHAN);
    pwmTimer.enableChannel(LED_PWM_RED_CHAN);

    pwmTimer.setDuty(LED_PWM_RED_CHAN, 0);
    pwmTimer.setDuty(LED_PWM_GREEN_CHAN, 0);

    seqTimer.init(36000 / LEDSequencer::TICK_HZ, 1000);
    seqTimer.enableUpdateIsr();

    outRed.setControl(GPIOPin::OUT_ALT_2MHZ);
    outGreen.setControl(GPIOPin::OUT_ALT_2MHZ);
}

void LED::set(const LEDPattern *pattern, bool immediate)
{
    sequencer.setPattern(pattern, immediate);
}

IRQ_HANDLER ISR_FN(LED_SEQUENCER_TIM)()
{
    const HwTimer pwmTimer(&LED_PWM_TIM);
    const HwTimer seqTimer(&LED_SEQUENCER_TIM);

    seqTimer.clearStatus();

    LEDSequencer::LEDState state;
    LED::sequencer.tickISR(state);

    pwmTimer.setDuty(LED_PWM_RED_CHAN, state.red);
    pwmTimer.setDuty(LED_PWM_GREEN_CHAN, state.green);
}
