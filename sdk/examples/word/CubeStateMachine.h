#ifndef CUBESTATEMACHINE_H
#define CUBESTATEMACHINE_H

#include <sifteo.h>
#include "StateMachine.h"
#include "config.h"
#include "Anim.h"
#include "Constants.h"
#include "TileTransparencyLookup.h"


using namespace Sifteo;

// TODO remove in next merge commit
typedef Int2 Vec2;

enum CubeAnim
{
    CubeAnim_Main,
    CubeAnim_Hint,

    NumCubeAnims
};

// TODO drive machine by anim state only
class CubeStateMachine : public StateMachine
{
public:
    CubeStateMachine();
    void setVideoBuffer(VideoBuffer& vidBuf);
    CubeID getCube();

    virtual unsigned getNumStates() const;

    virtual unsigned onEvent(unsigned eventID, const EventData& data);
    virtual void update(float dt);
    void sendEventToRow(unsigned eventID, const EventData& data);

    void resetStateTime() { mStateTime = 0.0f; }

    unsigned getLetters(char *buffer, bool forPaint) const;
    unsigned getMetaLetters(char *buffer, bool forPaint) const;
    void queueAnim(AnimType anim, CubeAnim cubeAnim=CubeAnim_Main);
    void queueNextAnim(CubeAnim cubeAnim=CubeAnim_Main);
    void updateAnim(AnimParams *params = 0);
    AnimType getAnim() const { return mAnimTypes[CubeAnim_Main]; }

    bool canBeginWord();
    bool beginsWord(bool& isOld, char* wordBuffer, bool& isBonus);
    unsigned findRowLength();
    bool isConnectedToCubeOnSide(CubeID cubeIDStart, Side side=LEFT);
    bool hasNoNeighbors();
    float getIdleTime() const { return mIdleTime; }
    bool canNeighbor() const { return (int)mBG0Panning == (int)mBG0TargetPanning; }
    int getPanning() const { return (int)mBG0Panning; }

    static unsigned findNumLetters(char *string);
    bool canStartHint() const;
    bool canUseHint() const;

    void startHint() { queueAnim(AnimType_HintWindUpSlide, CubeAnim_Hint); }
    void stopHint() { queueAnim(AnimType_HintWindUpSlide, CubeAnim_Hint); }

protected:
    void setState(unsigned newStateIndex, unsigned oldStateIndex);

private:
    void setPanning(float panning);
    AnimType getNextAnim(CubeAnim cubeAnim=CubeAnim_Main);
    void paint();

    void paintScore(ImageIndex teethImageIndex,
                    bool animate=false,
                    bool reverseAnim=false,
                    bool loopAnim=false,
                    bool paintTime=false,
                    float animStartTime=0.f);
    void paintLetters(const AssetImage &font, bool paintSprites=false);
    void paintScoreNumbers(const Vec2& position, const char* string);

    void setLettersStart(unsigned s);

    bool getAnimParams(AnimParams *params);
    void calcSpriteParams(unsigned i);
    void updateSpriteParams(float dt);
    bool calcHintTiltDirection(unsigned &newLettersStart,
                               unsigned &tiltDirection) const;

    // shared state data
    char mLetters[MAX_LETTERS_PER_CUBE + 1];
    char mHintSolution[MAX_LETTERS_PER_CUBE + 1];
    char mMetaLetters[MAX_LETTERS_PER_CUBE + 1];
    Vec2 mTilePositions[MAX_LETTERS_PER_CUBE];
    unsigned char mPuzzleLettersPerCube;
    unsigned char mPuzzlePieceIndex;
    unsigned char mMetaPieceIndex;
    unsigned char mMetaLettersStart;
    unsigned char mMetaLettersStartOld;
    unsigned char mMetaLettersPerCube;
    float mIdleTime;
    bool mNewHint;
    AnimType mAnimTypes[NumCubeAnims];
    float mAnimTimes[NumCubeAnims];

    bool mPainting;

    float mBG0Panning;
    float mBG0TargetPanning;
    bool mBG0PanningLocked;
    unsigned mLettersStart;
    unsigned mLettersStartOld;

    ImageIndex mImageIndex;
    SpriteParams mSpriteParams;

    VideoBuffer* mVidBuf;
    float mShakeDelay;
    float mPanning;
    float mTouchHoldTime;
    bool mTouchHoldWaitForUntouch;
};

#endif // CUBESTATEMACHINE_H
