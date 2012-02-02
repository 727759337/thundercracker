#include <sifteo.h>
#include "EventID.h"
#include "EventData.h"
#include "assets.gen.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include "ScoredCubeState_NewWord.h"
#include "WordGame.h"

ScoredCubeState_NewWord::ScoredCubeState_NewWord() : mImageIndex(ImageIndex_ConnectedWord)
{
}

unsigned ScoredCubeState_NewWord::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    case EventID_NewWordFound:
        if (getStateMachine().isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            getStateMachine().resetStateTime();
        }
        // fall through
    case EventID_EnterState:
        {
            Cube& c = getStateMachine().getCube();
            mImageIndex = ImageIndex_ConnectedWord;
            if (c.physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
                c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedLeftWord;
            }
            else if (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
                     c.physicalNeighborAt(SIDE_RIGHT) == CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedRightWord;
            }
        }
        WordGame::instance()->setNeedsPaintSync();
        // fall through

    case EventID_Paint:
    case EventID_ClockTick:
        paint();
        break;

    case EventID_AddNeighbor:
    case EventID_RemoveNeighbor:
    case EventID_LetterOrderChange:
        paint();
        break;

        /* TODO remove dead code
    case EventID_WordBroken:
        if (!getStateMachine().canBeginWord() &&
            getStateMachine().isConnectedToCubeOnSide(data.mWordBroken.mCubeIDStart))
        {
            return CubeStateIndex_NotWordScored;
        }
        break;


    case EventID_OldWordFound:
        if (!getStateMachine().canBeginWord() &&
             getStateMachine().isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            return CubeStateIndex_OldWordScored;
        }
        break;
*/
    case EventID_GameStateChanged:
        switch (data.mGameStateChanged.mNewStateIndex)
        {
        case GameStateIndex_EndOfRoundScored:
            return CubeStateIndex_EndOfRoundScored;

        case GameStateIndex_ShuffleScored:
            return CubeStateIndex_ShuffleScored;
        }
        break;
    }
    return getStateMachine().getCurrentStateIndex();
}

unsigned ScoredCubeState_NewWord::update(float dt, float stateTime)
{
    CubeState::update(dt, stateTime);
    if (getStateMachine().getTime() <= TEETH_ANIM_LENGTH)
    {
        return CubeStateIndex_NewWordScored;
    }
    else if (GameStateMachine::getNumAnagramsRemaining() <= 0)
    {
        return CubeStateIndex_ShuffleScored;
    }
    else
    {
        bool isOldWord = false;
        if (getStateMachine().canBeginWord())
        {
            char wordBuffer[MAX_LETTERS_PER_WORD + 1];
            EventData wordFoundData;
            if (getStateMachine().beginsWord(isOldWord, wordBuffer, wordFoundData.mWordFound.mBonus))
            {
                wordFoundData.mWordFound.mCubeIDStart = getStateMachine().getCube().id();
                wordFoundData.mWordFound.mWord = wordBuffer;

                if (isOldWord)
                {
                    GameStateMachine::sOnEvent(EventID_OldWordFound, wordFoundData);
                    return CubeStateIndex_OldWordScored;
                }
                else
                {
                    GameStateMachine::sOnEvent(EventID_NewWordFound, wordFoundData);
                    return CubeStateIndex_NewWordScored;
                }
            }
            else
            {
                EventData wordBrokenData;
                wordBrokenData.mWordBroken.mCubeIDStart = getStateMachine().getCube().id();
                GameStateMachine::sOnEvent(EventID_WordBroken, wordBrokenData);
                return CubeStateIndex_NotWordScored;
            }
        }
        else if (getStateMachine().hasNoNeighbors())
        {
            return CubeStateIndex_NotWordScored;
        }
        return CubeStateIndex_OldWordScored;
    }
}

void ScoredCubeState_NewWord::paint()
{
    Cube& c = getStateMachine().getCube();
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    if (GameStateMachine::getCurrentMaxLettersPerCube() == 1)
    {
        paintLetters(vid, Font1Letter, true);
    }
    else
    {
        vid.BG0_drawAsset(Vec2(0,0), ScreenOff);
    }

    paintTeeth(vid, mImageIndex, true, false, false, false);
    vid.BG0_setPanning(Vec2(0.f, 0.f));
}
