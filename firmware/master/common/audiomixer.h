/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef AUDIOMIXER_H_
#define AUDIOMIXER_H_

#include "ringbuffer.h"
#include "audiochannel.h"
#include <stdint.h>
#include <stdio.h>
#include <sifteo/abi.h>

class AudioMixer
{
public:
    // Global sample rate for mixing and audio output
    static const unsigned SAMPLE_HZ = 16000;

    // Type and size for output buffer, between mixer and audio device
    typedef RingBuffer<1024, int16_t> OutputBuffer;

    AudioMixer();

    void init();

    static AudioMixer instance;
    static OutputBuffer output;

    static void test();

    bool play(const struct _SYSAudioModule *mod, _SYSAudioChannelID ch,
        _SYSAudioLoopType loopMode = _SYS_LOOP_ONCE);
    bool isPlaying(_SYSAudioChannelID ch) const;
    void stop(_SYSAudioChannelID ch);

    void pause(_SYSAudioChannelID ch);
    void resume(_SYSAudioChannelID ch);

    void setVolume(_SYSAudioChannelID ch, uint16_t volume);
    int volume(_SYSAudioChannelID ch) const;

    void setSpeed(_SYSAudioChannelID ch, uint32_t samplerate);

    void setPos(_SYSAudioChannelID ch, uint32_t ofs);
    uint32_t pos(_SYSAudioChannelID ch) const;

    void setLoop(_SYSAudioChannelID ch, _SYSAudioLoopType loopMode);

    ALWAYS_INLINE bool outputBufferIsSilent() const {
        // + 1 to account for the fact that the physical size of the
        // ring buffer is 1 greater than its capacity().
        return numSilentSamples > output.capacity() + 1;
    }

    ALWAYS_INLINE bool active() const {
        /*
         * We like to pause the mixer when no sound is playing, to conserve CPU and power.
         *
         * We can only pause the mixer when the output buffer is filled with
         * silence. Any time the mixer wakes up, we need to re-zero the buffer
         * before it goes back to sleep. This is to support output devices which
         * do a looping DMA transfer from the buffer.
         */
        return playingChannelMask != 0 || !outputBufferIsSilent();
    }

    static void pullAudio();

    ALWAYS_INLINE unsigned channelID(AudioChannelSlot *slot) {
        return slot - &channelSlots[0];
    }

protected:
    friend class XmTrackerPlayer; // can call setTrackerCallbackInterval()
    void setTrackerCallbackInterval(uint32_t usec);

private:
    // Tracker callback timer
    uint32_t trackerCallbackInterval;
    uint32_t trackerCallbackCountdown;

    uint32_t playingChannelMask;    // channels that are actively playing
    uint32_t numSilentSamples;      // Number of samples in 'output' guaranteed to be silent

    AudioChannelSlot channelSlots[_SYS_AUDIO_MAX_CHANNELS];

    bool mixAudio(int *buffer, uint32_t numFrames);

    static int softLimiter(int32_t sample);
};

#endif /* AUDIOMIXER_H_ */
