#include <sifteo.h>
#include "EventID.h"
#include "EventData.h"
#include "assets.gen.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include "ScoredCubeState_NewWord.h"
#include <string.h>

ScoredCubeState_NewWord::ScoredCubeState_NewWord()
{
}

unsigned ScoredCubeState_NewWord::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    // TODO debug: case EventID_Paint:
    case EventID_EnterState:
    case EventID_Paint:
    case EventID_ClockTick:
    case EventID_NewWordFound:
        paint();
        break;

    case EventID_AddNeighbor:
    case EventID_RemoveNeighbor:
        {
            bool isOldWord = false;
            if (getStateMachine().canBeginWord())
            {
                char wordBuffer[MAX_LETTERS_PER_WORD + 1];
                if (getStateMachine().beginsWord(isOldWord, wordBuffer))
                {
                    EventData data;
                    data.mWordFound.mCubeIDStart = getStateMachine().getCube().id();
                    data.mWordFound.mWord = wordBuffer;

                    if (isOldWord)
                    {
                        GameStateMachine::sOnEvent(EventID_OldWordFound, data);
                        return CubeStateIndex_OldWordScored;
                    }
                    else
                    {
                        GameStateMachine::sOnEvent(EventID_NewWordFound, data);
                        return CubeStateIndex_NewWordScored;
                    }
                }
                else
                {
                    EventData data;
                    data.mWordBroken.mCubeIDStart = getStateMachine().getCube().id();
                    GameStateMachine::sOnEvent(EventID_WordBroken, data);
                    return CubeStateIndex_NotWordScored;
                }
            }
            else if (getStateMachine().hasNoNeighbors())
            {
                return CubeStateIndex_NotWordScored;
            }
            paint();
        }
        break;

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

    case EventID_EndRound:
        return CubeStateIndex_EndOfRoundScored;
    }
    return getStateMachine().getCurrentStateIndex();
}

unsigned ScoredCubeState_NewWord::update(float dt, float stateTime)
{
    return getStateMachine().getTime() < 0.5f ?
                CubeStateIndex_NewWordScored : CubeStateIndex_OldWordScored;
}

void ScoredCubeState_NewWord::paint()
{
    Cube& c = getStateMachine().getCube();
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    paintLetters(vid, Font1Letter, true);

    ImageIndex ii = ImageIndex_ConnectedWord;
    if (c.physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
        c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED)
    {
        ii = ImageIndex_ConnectedLeftWord;
    }
    else if (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
             c.physicalNeighborAt(SIDE_RIGHT) == CUBE_ID_UNDEFINED)
    {
        ii = ImageIndex_ConnectedRightWord;
    }

    paintTeeth(vid, ii, true, false, false, false);
}
