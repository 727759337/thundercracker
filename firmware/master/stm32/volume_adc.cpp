
#include "volume.h"
#include "board.h"
#include "gpio.h"
#include "macros.h"
#include "adc.h"
#include "batterylevel.h"
#include <sifteo/abi/audio.h>

#ifdef USE_ADC_FADER_MEAS

/*
 * Volume fader measurement via ADC.
 *
 * Used as of rev3, since we're now running at a voltage
 * that supports the ADC hardware.
 *
 * Earlier hardware revs make use of volume_rc.cpp
 */

static uint32_t calibratedMin;
static uint64_t calibratedScale;


namespace Volume {

static bool charging;
static unsigned lastReading;

void init()
{
    charging = false;
    lastReading = 0;

    GPIOPin faderEnable = FADER_MEAS_EN;
    faderEnable.setControl(GPIOPin::OUT_2MHZ);
    faderEnable.setHigh();

    GPIOPin faderMeas = FADER_MEAS_GPIO;
    faderMeas.setControl(GPIOPin::IN_ANALOG);

    FADER_ADC.setCallback(FADER_ADC_CHAN, Volume::adcCallback);
    FADER_ADC.setSampleRate(FADER_ADC_CHAN, Adc::SampleRate_55_5);

}

/*
 *  Scale values in [FADER_MIN, FADER_MAX] to [0, MAX_VOLUME]
 *  Any values beyond FADER_MIN/MAX get clamped
 */
int systemVolume()
{
    return clamp((int)(lastReading * MAX_VOLUME/(FADER_MAX - FADER_MIN)), 0, MAX_VOLUME);
}

void beginCapture()
{
    /*
     * As a power saving measure, we only provide power
     * to the fader when we want to take a measurement.
     *
     * We need a little charge time before taking a sample,
     * so we alternate between enabling the fader and
     * actually kicking off the ADC sample each time
     * we get called here.
     */

    GPIOPin faderEnable = FADER_MEAS_EN;

    if (charging) {
        FADER_ADC.beginSample(FADER_ADC_CHAN);
        charging = false;

    } else {
        faderEnable.setControl(GPIOPin::OUT_2MHZ);
        faderEnable.setHigh();
        charging = true;
    }
}

int calibrate(CalibrationState state)
{

    switch (state) {
    case CalibrationLow:
        break;
    case CalibrationHigh:
        break;
    }

    return lastReading;
}

void adcCallback(uint16_t sample) {
    lastReading = sample;

     GPIOPin faderEnable = FADER_MEAS_EN;
     faderEnable.setControl(GPIOPin::IN_FLOAT);

#ifdef USE_ADC_BATT_MEAS
    BatteryLevel::beginCapture();
#endif
}

} // namespace Volume

#endif // USE_ADC_FADER_MEAS
