/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef AUDIOMIXER_H_
#define AUDIOMIXER_H_

#include "audiobuffer.h"
#include "audiochannel.h"
#include <stdint.h>
#include <stdio.h>

class AudioMixer
{
public:
    AudioMixer();

    static AudioMixer instance;

    void init();
    void enableChannel(struct _SYSAudioBuffer *buffer);

    static void test();

    bool play(const struct _SYSAudioModule *mod, _SYSAudioHandle *handle, _SYSAudioLoopType loopMode = LoopOnce);
    bool isPlaying(_SYSAudioHandle handle);
    void stop(_SYSAudioHandle handle);

    void pause(_SYSAudioHandle handle);
    void resume(_SYSAudioHandle handle);

    void setVolume(_SYSAudioHandle handle, int volume);
    int volume(_SYSAudioHandle handle);

    uint32_t pos(_SYSAudioHandle handle);

    bool active() const { return playingChannelMask != 0; }

    int pullAudio(int16_t *buffer, int numsamples);
    void fetchData();
    static void handleAudioOutEmpty(void *p);

private:
    uint32_t enabledChannelMask;    // channels userspace has provided buffers for
    uint32_t playingChannelMask;    // channels that are actively playing

    _SYSAudioHandle nextHandle;

    AudioChannelSlot channelSlots[_SYS_AUDIO_MAX_CHANNELS];

    AudioChannelSlot* channelForHandle(_SYSAudioHandle handle, uint32_t mask);
};

#endif /* AUDIOMIXER_H_ */
