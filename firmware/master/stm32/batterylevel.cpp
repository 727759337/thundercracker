#include "batterylevel.h"
#include "rctimer.h"
#include "board.h"
#include "neighbor_tx.h"
#include "neighbor_protocol.h"
#include "powermanager.h"
#include "macros.h"

namespace BatteryLevel {

enum State {
    Capture,
    Calibration,
};

static unsigned lastReading;
static unsigned lastVsysReading;
static State currentState;

/*
 * As we're sharing a timer with NeighborTX, the end of each neighbor transmission
 * is an opportunity for us to begin a sample. Lets wait 2.5 seconds between
 * each sample. The minimum amount of wait time is 75 ms (DELAY_PRESCALER ~= 25)
 *
 * 2500 / (NUM_TX_WAIT_PERIODS * BIT_PERIOD_TICKS * TICK) ~= 833
 *
 * See neighbor_protocol.h for neighbor transmission periods.
 */

static const unsigned DELAY_PRESCALER = 833;
// no delay for first sample - want to capture initial value immediately
static unsigned delayPrescaleCounter = DELAY_PRESCALER;

/*
 * Max discharge time is 3ms.
 * To configure our timer at this resolution, we need to use the maximum
 * period (0xffff), and select a prescaler that gets us as close as possible.
 *
 * 0.05 / ((1 / 36000000) * 0xffff) == 27.466
 */
static const unsigned DISCHARGE_PRESCALER = 3;



void init()
{
    /*
     * Setting lastReading for comparison on startup
     */
    lastReading = UNINITIALIZED;
    lastVsysReading = UNINITIALIZED;

    /*
     * Set the current state
     */
    currentState = Capture;

    /*
     * BATT_MEAS can always remain configured as an input.
     * To charge (default state), BATT_MEAS_GND => input/float.
     * To discharge, BATT_MEAS_GND => driven low and timer capture on BATT_MEAS.
     */

    BATT_MEAS_GPIO.setControl(GPIOPin::IN_FLOAT);

    GPIOPin battMeasGnd = BATT_MEAS_GND_GPIO;
    battMeasGnd.setControl(GPIOPin::IN_FLOAT);
    battMeasGnd.setLow();

    /*
     * We share this timer with NeighborTX - its alrady been init'd there.
     * Just configure our channel, but don't enable it's ISR yet.
     */

    HwTimer timer(&BATT_LVL_TIM);
    timer.configureChannelAsInput(BATT_LVL_CHAN, HwTimer::FallingEdge);
    timer.enableChannel(BATT_LVL_CHAN);
}

unsigned raw()
{
    return lastReading;
}

unsigned vsys()
{
    return lastVsysReading;
}

unsigned scaled()
{
    /*
     * Temporary cheesy linear scaling.
     *
     * We need some battery curves, and then we'd like to chunk our values into
     * the number of visual buckets that the UI represents.
     */
    const unsigned MAX = 0x2500;
    const unsigned MIN = 0x1baa;
    const unsigned RANGE = MAX - MIN;
    const unsigned clamped = clamp(lastReading, MIN, MAX);
    return (clamped - STARTUP_THRESHOLD) * _SYS_BATTERY_MAX / RANGE;
}

void beginCapture()
{
    /*
     * An opportunity to take a new sample.
     *
     * First, detect whether a battery is connected: BATT_MEAS will be high.
     *
     * BATT_LVL_TIM is shared with neighbor transmit
     */

    if (currentState == Calibration ||
        delayPrescaleCounter++ == DELAY_PRESCALER)
    {

        delayPrescaleCounter = 0;

        //Returns if there is no battery or if USB is connected
        if (BATT_MEAS_GPIO.isLow() ||
            PowerManager::state() == PowerManager::UsbPwr)
        {
            return;
        }

        NeighborTX::pause();

        HwTimer timer(&BATT_LVL_TIM);
        timer.setPeriod(0xffff, DISCHARGE_PRESCALER);
        timer.setCount(0);

        /*
         * If the current state is the calibration state
         * then BATT_MEAS_GPIO was set as an output
         * in the captureISR
         */
        if (currentState == Calibration) {
            BATT_MEAS_GPIO.setControl(GPIOPin::IN_FLOAT);
        }

        BATT_MEAS_GND_GPIO.setControl(GPIOPin::OUT_2MHZ);
        BATT_MEAS_GND_GPIO.setLow();

        timer.enableCompareCaptureIsr(BATT_LVL_CHAN);
    }
}

void captureIsr()
{
    /*
     * Called from within NeighborTX, where the interrupts for the
     * timer we're on are handled.
     *
     * Once our discharge has completed, we can resume normal neighbor transmission.
     */
    BATT_MEAS_GND_GPIO.setControl(GPIOPin::IN_FLOAT);

    HwTimer timer(&BATT_LVL_TIM);
    timer.disableCompareCaptureIsr(BATT_LVL_CHAN);

    /*
     * We don't need to track a start time, as we always reset the timer
     * before kicking off a new capture.
     *
     * We alternately sample VSYS and VBATT in order to establish a consistent
     * baseline - store the capture appropriately.
     */

    unsigned capture = timer.lastCapture(BATT_LVL_CHAN);

    if (currentState == Capture) {
        lastReading = capture;

        BATT_MEAS_GPIO.setControl(GPIOPin::OUT_2MHZ);
        BATT_MEAS_GPIO.setHigh();

        currentState = Calibration;

    } else if  (currentState == Calibration) {
        lastVsysReading = capture;

        currentState = Capture;
    }

    NeighborTX::resume();
}

} // namespace BatteryLevel
