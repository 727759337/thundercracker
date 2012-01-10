#ifndef DAC_AUDIO_OUT_H
#define DAC_AUDIO_OUT_H

#include "dac.h"
#include "gpio.h"
#include "hardware.h"
#include "hwtimer.h"

#include "audiooutdevice.h"
#include "speexdecoder.h"
class AudioMixer;

class DacAudioOut
{
public:
    DacAudioOut(int dacChannel, HwTimer _sampleTimer) :
        dac(Dac::instance),
        dacChan(dacChannel),
        sampleTimer(_sampleTimer)
    {}
    void init(AudioOutDevice::SampleRate samplerate, AudioMixer *mixer, GPIOPin &output);

    void start();
    void stop();

    bool isBusy() const;

    int sampleRate() const;
    void setSampleRate(AudioOutDevice::SampleRate samplerate);

    void suspend();
    void resume();

    void tmrIsr();

private:
    Dac &dac;
    int dacChan;
    HwTimer sampleTimer;

    typedef struct AudioOutBuffer_t {
        int16_t data[SpeexDecoder::DECODED_FRAME_SIZE];
        int count;
        int offset;
    } AudioOutBuffer;

    AudioMixer *mixer;
    AudioOutBuffer audioBufs[1];

    void dmaIsr(uint32_t flags);
//    void tmrIsr();
    friend class AudioOutDevice;
};

#endif // DAC_AUDIO_OUT_H
