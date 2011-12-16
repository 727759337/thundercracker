#include <sifteo.h>
#include "EventID.h"
#include "EventData.h"
#include "assets.gen.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include <string.h>
#include "ScoredCubeState_OldWord.h"

ScoredCubeState_OldWord::ScoredCubeState_OldWord()
{
}

unsigned ScoredCubeState_OldWord::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    // TODO debug: case EventID_Paint:
    case EventID_EnterState:
    case EventID_Paint:
    case EventID_ClockTick:
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

    case EventID_NewWordFound:
        if (!getStateMachine().canBeginWord() &&
             getStateMachine().isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            return CubeStateIndex_NewWordScored;
        }
        break;

    case EventID_EndRound:
        return CubeStateIndex_EndOfRoundScored;
    }
    return getStateMachine().getCurrentStateIndex();
}

unsigned ScoredCubeState_OldWord::update(float dt, float stateTime)
{
    return getStateMachine().getCurrentStateIndex();
}

void ScoredCubeState_OldWord::paint()
{
    Cube& c = getStateMachine().getCube();
    // FIXME vertical words
    const Sifteo::AssetImage& bg =
        (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
         c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED) ?
            BGOldWordConnectedMiddle :
            (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED) ?
                BGOldWordConnectedRight:
                BGOldWordConnectedLeft;
    VidMode_BG0 vid(c.vbuf);
    vid.init();
    vid.BG0_drawAsset(Vec2(0,0), bg);
    paintLetters(vid, FontRepeat);
    char string[5];
    sprintf(string, "%d", GameStateMachine::GetSecondsLeft());
#if DEBUGZZZZZZZZ
    printf("%d %s\n", getStateMachine().getCube().id(), string);
#endif
    vid.BG0_text(Vec2(5 + (4 - strlen(string)), 14), FontSmall, string);

    paintTeeth();
}
