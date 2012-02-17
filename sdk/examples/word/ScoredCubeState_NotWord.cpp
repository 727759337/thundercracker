#include <sifteo.h>
#include "EventID.h"
#include "EventData.h"
#include "assets.gen.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include "Dictionary.h"
#include "ScoredCubeState_NotWord.h"
#include "WordGame.h"

ScoredCubeState_NotWord::ScoredCubeState_NotWord()
{
}

unsigned ScoredCubeState_NotWord::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    case EventID_EnterState:
    case EventID_NewAnagram:
    case EventID_Paint:
    case EventID_ClockTick:
        paint();
        break;

    case EventID_AddNeighbor:
    case EventID_RemoveNeighbor:
    case EventID_LetterOrderChange:
        {
            bool isOldWord = false;
            if (getStateMachine().canBeginWord())
            {

                char wordBuffer[MAX_LETTERS_PER_WORD + 1];
                EventData wordFoundData;
                if (getStateMachine().beginsWord(isOldWord, (char*)wordBuffer, wordFoundData.mWordFound.mBonus))
                {
                    wordFoundData.mWordFound.mCubeIDStart = getStateMachine().getCube().id();
                    wordFoundData.mWordFound.mWord = wordBuffer;

                    if (isOldWord)
                    {
                        GameStateMachine::sOnEvent(EventID_OldWordFound, wordFoundData);
                        return CubeStateIndex_OldWordScored;
                    }

                    GameStateMachine::sOnEvent(EventID_NewWordFound, wordFoundData);
                    return CubeStateIndex_NewWordScored;
                }
                else
                {
                    EventData wordBrokenData;
                    wordBrokenData.mWordBroken.mCubeIDStart = getStateMachine().getCube().id();
                    GameStateMachine::sOnEvent(EventID_WordBroken, wordBrokenData);

                    return CubeStateIndex_NotWordScored;
                }
                paint();
            }
        }
        break;

    case EventID_NewWordFound:
        if (!getStateMachine().canBeginWord() &&
             getStateMachine().isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            return CubeStateIndex_NewWordScored;
        }
        break;

    case EventID_OldWordFound:
        if (!getStateMachine().canBeginWord() &&
             getStateMachine().isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            return CubeStateIndex_OldWordScored;
        }
        break;

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

unsigned ScoredCubeState_NotWord::update(float dt, float stateTime)
{
    CubeState::update(dt, stateTime);
    return getStateMachine().getCurrentStateIndex();
}

void ScoredCubeState_NotWord::paint()
{
    Cube& c = getStateMachine().getCube();
    // FIXME vertical words
    bool neighbored =
            (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED ||
            c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED);
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    switch (GameStateMachine::getCurrentMaxLettersPerCube())
    {
    case 2:
        paintLetters(vid, Font2Letter, true);
        break;

    case 3:
        paintLetters(vid, Font3Letter, true);
        break;

    default:
        paintLetters(vid, Font1Letter, true);
        break;
    }

    if (neighbored)
    {
        paintBorder(vid, ImageIndex_Neighbored, true, false, true, false);
    }
    else
    {
        paintBorder(vid, ImageIndex_Teeth, false, true, false, true);
    }
}
