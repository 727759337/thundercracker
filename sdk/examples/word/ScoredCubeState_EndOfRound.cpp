#include "ScoredCubeState_EndOfRound.h"

#include <sifteo.h>
#include "EventID.h"
#include "EventData.h"
#include "assets.gen.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include "ScoredCubeState_EndOfRound.h"
#include "SavedData.h"

unsigned ScoredCubeState_EndOfRound::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    // TODO debug: case EventID_Paint:
    case EventID_EnterState:
    case EventID_Paint:
        paint();
        break;

    case EventID_NewAnagram:
        // fired from ScoredGameState::onEvent on _Input
        return CubeStateIndex_NotWordScored;

    }
    return getStateMachine().getCurrentStateIndex();
}

unsigned ScoredCubeState_EndOfRound::update(float dt, float stateTime)
{
    return getStateMachine().getCurrentStateIndex();
}

void ScoredCubeState_EndOfRound::paint()
{
    Cube& c = getStateMachine().getCube();
    // FIXME vertical words
    bool neighbored =
            (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED ||
            c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED);
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    if (GameStateMachine::getTime() <= TEETH_ANIM_LENGTH)
    {
        //        paintLetters(vid, (neighbored ? FontNeighbored : FontUnneighbored));
        paintLetters(vid, FontUnneighbored);
        paintTeeth(vid, true, false);
        return;
    }

    switch (getStateMachine().getCube().id())
    {
    case 0:
        // TODO paint "Score" asset
        vid.BG0_drawAsset(Vec2(0,0), BGNotWordNotConnected);
        char string[17];
        sprintf(string, "%.5d", GameStateMachine::getScore());
        paintScoreNumbers(vid, Vec2(5,6), string);
        break;

    case 1:
        vid.BG0_drawAsset(Vec2(0,0), StartScreen);
        break;

    default:
        // TODO paint "high scores" asset
        vid.BG0_drawAsset(Vec2(0,0), BGNotWordNotConnected);
        for (unsigned i = arraysize(SavedData::sHighScores) - 1;
             i >= 0;
             --i)
        {
            if (SavedData::sHighScores[i] == 0)
            {
                break;
            }
            char string[17];
            sprintf(string, "%.5d", SavedData::sHighScores[i]);
            paintScoreNumbers(vid, Vec2(5,4 + (arraysize(SavedData::sHighScores) - i) * 2),
                         string);
        }
        break;
    }
}
