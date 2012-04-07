/*
 * This file is part of the public interface to the Sifteo SDK.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _SIFTEO_AUDIO_H_
#define _SIFTEO_AUDIO_H_

#ifdef NO_USERSPACE_HEADERS
#   error This is a userspace-only header, not allowed by the current build.
#endif

#include <sifteo/asset.h>
#include <sifteo/abi.h>

namespace Sifteo {


class AudioChannel {
public:

    AudioChannel() : handle(_SYS_AUDIO_INVALID_HANDLE)
    {}

    bool play(const AssetAudio &mod, _SYSAudioLoopType loopMode = _SYS_LOOP_ONCE) {
        return _SYS_audio_play(&mod.sys, &handle, loopMode);
    }

    bool isPlaying() const {
        return _SYS_audio_isPlaying(handle);
    }

    void stop() {
        _SYS_audio_stop(handle);
    }

    void pause() {
        _SYS_audio_pause(handle);
    }

    void resume() {
        _SYS_audio_resume(handle);
    }

    void setVolume(int volume) {
        _SYS_audio_setVolume(handle, volume);
    }

    int volume() {
        return _SYS_audio_volume(handle);
    }

    uint32_t pos() {
        return _SYS_audio_pos(handle);
    }

private:
    _SYSAudioHandle handle;
};


} // namespace Sifteo

#endif // AUDIO_H_
