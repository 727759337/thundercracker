#ifndef WORDGAME_H
#define WORDGAME_H

#include <sifteo.h>
#include "GameStateMachine.h"
using namespace Sifteo;

union EventData;

enum AudioChannelIndex
{
    AudioChannelIndex_Music,
    AudioChannelIndex_Neighbor = 1,
    AudioChannelIndex_Score = 1,   // HACK only 2 channels are funcitoning for now
    AudioChannelIndex_Bonus = 1,
    AudioChannelIndex_Teeth = 1,
    AudioChannelIndex_Shake = 1,
    AudioChannelIndex_Time = 1,

    NumAudioChannelIndexes
};

class WordGame
{
public:
    WordGame(Cube cubes[]);
    void update(float dt);

    bool needsPaintSync() const { return mNeedsPaintSync; }
    void setNeedsPaintSync() { mNeedsPaintSync = true; }
    void paintSync() { System::paintSync(); mNeedsPaintSync = false; }

    static WordGame* instance() { return sInstance; }

    static void hideSprites(VidMode_BG0_SPR_BG1 &vid);
    static void onEvent(unsigned eventID, const EventData& data);
    static bool playAudio(_SYSAudioModule &mod, AudioChannelIndex channel = AudioChannelIndex_Music, _SYSAudioLoopType loopMode = LoopOnce);
    static unsigned rand(unsigned max);
    static float rand(float min, float max);
    static void seedRand(unsigned seed);

private:
    bool _playAudio(_SYSAudioModule &mod, AudioChannelIndex channel = AudioChannelIndex_Music, _SYSAudioLoopType loopMode = LoopOnce);
    void _onEvent(unsigned eventID, const EventData& data);

    GameStateMachine mGameStateMachine;
    AudioChannel mAudioChannels[NumAudioChannelIndexes];
    unsigned mRandomSeed;
    bool mNeedsPaintSync;
    static WordGame* sInstance;
};

#endif // WORDGAME_H
