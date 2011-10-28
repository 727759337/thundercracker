/*
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef AUDIOMIXER_H_
#define AUDIOMIXER_H_

#include <stdint.h>
#include "speexdecoder.h"

class AudioMixer
{
public:
    typedef uint8_t module_t;       // ID for a particular asset
    typedef uint32_t handle_t;      // handle for a playback instance

    static const int NUM_CHANNELS = 8;

    enum LoopMode {
        LoopOnce,
        LoopRepeat
    };

    AudioMixer();

    void init();

    static void test();

    void setSoundBank(uintptr_t address);

    bool play(module_t mod, handle_t *handle, enum LoopMode loopMode = LoopOnce);
    bool isPlaying(handle_t handle);
    void stop(handle_t mod);

    void pause(handle_t mod);
    void resume(handle_t mod);

    void setVolume(handle_t mod, int volume);
    int volume(handle_t mod);

    uint32_t pos(handle_t mod);

    bool active() const { return activeChannelMask; }

    int pullAudio(char *buffer, int numsamples);
    void fetchData();

private:
    typedef struct AudioChannel_t {
        SpeexDecoder decoder; // tbd how this actually gets represented...

        bool endOfStream() const {
            return decoder.endOfStream();
        }

        int volume;
        handle_t handle;
        uint8_t state;

        char *ptr;
        char decompressBuf[SpeexDecoder::DECODED_FRAME_SIZE]; // XXX tune size
        int decompressedSize;

        int bytesToMix() const {
            return decompressedSize - (ptr - decompressBuf);
        }
    } AudioChannel;

    static const int STATE_PAUSED   = (1 << 0);
    static const int STATE_LOOP     = (1 << 1);

    uint32_t activeChannelMask;
    handle_t nextHandle;
    AudioChannel channels[NUM_CHANNELS];

    AudioChannel* channelForHandle(module_t mod);
    void stopChannel(AudioChannel *ch);
};

#endif /* AUDIOMIXER_H_ */
