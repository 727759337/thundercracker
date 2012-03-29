#ifndef ANIM_H
#define ANIM_H

#include <sifteo.h>
#include "LevelProgressData.h"
#include "Constants.h"

enum AnimType
{
    AnimType_None = -1,

    AnimType_NotWord,
    AnimType_SlideL,
    AnimType_SlideR,
    AnimType_OldWord,
    AnimType_NewWord,
    AnimType_EndofRound,
    AnimType_Shuffle,
    AnimType_CityProgression,
    AnimType_HintBarAppear,
    AnimType_HintBarIdle,
    AnimType_HintBarDisappear,
    AnimType_HintWindUpSlide,
    AnimType_HintSlideL,
    AnimType_HintSlideR,
    AnimType_NormalTilesEnter,
    AnimType_NormalTilesExit,
    AnimType_MetaTilesEnter,
    AnimType_MetaTilesShow,
    AnimType_MetaTilesExit,

    NumAnimTypes
};

struct SpriteParams
{
    Float2 mPositions[_SYS_VRAM_SPRITES];
    Float2 mEndPositions[_SYS_VRAM_SPRITES];
    float mStartDelay[_SYS_VRAM_SPRITES];
};

struct AnimParams
{
    char mLetters[MAX_LETTERS_PER_CUBE + 1];
    bool mLeftNeighbor, mRightNeighbor;
    Cube::ID mCubeID;
    bool mBorders;
    bool mBonus;
    char mMetaLetterIndex;
    char mLettersPerCube;
    bool mAllMetaLetters;
    SpriteParams *mSpriteParams;
};

bool animPaint(AnimType anim,
               VidMode_BG0_SPR_BG1 &vid,
               BG1Helper *bg1 = 0,
               float animTime = 0.f,
               const AnimParams *params=0);

#endif // ANIM_H
