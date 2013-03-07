#ifndef BATTERYLEVEL_H
#define BATTERYLEVEL_H

#include <sifteo/abi.h>
#include <stdint.h>
#include "bits.h"

namespace BatteryLevel
{
    static const uint8_t BASE = _SYS_NUM_CUBE_SLOTS;
    static const uint8_t NONE = _SYS_NUM_CUBE_SLOTS+1;

    void init();
    unsigned raw();
    unsigned vsys();
    unsigned scaled(uint8_t cubeNum = BASE); // master by default
    void beginCapture();
    void captureIsr();
    void process(unsigned);
    uint8_t getLowBatDevice();
    bool needWarning();
    void setWarningDone(uint8_t cubeNum);
    void onCapture();

    // simulated only :
    void heartbeat();
    void updatePercentage(int8_t delta, uint8_t cubeNum);
    uint8_t getPercentage(uint8_t cubeNum);

    // In the following arrays, the last element refers to the master cube:
    static uint8_t lowBatDevice = NONE;
    static BitVector<_SYS_NUM_CUBE_SLOTS+1> warningDone;

    /*
     * Sentinel value to help determine whether a sample has successfully
     * been taken after startup.
     */
    static const unsigned UNINITIALIZED = 0xffff;

    /*
     * Empirically measured maximum jitter in battery level readings
     */
    static const unsigned MAX_JITTER = 0x81;
}

#endif // BATTERYLEVEL_H
