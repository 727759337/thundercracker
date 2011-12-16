#ifndef CUBESTATE_H
#define CUBESTATE_H

#include <sifteo.h>
#include "State.h"

enum CubeStateIndex
{
    CubeStateIndex_Title,
    CubeStateIndex_TitleExit,
    CubeStateIndex_NotWordScored,
    CubeStateIndex_NewWordScored,
    CubeStateIndex_OldWordScored,
    CubeStateIndex_EndOfRoundScored,

    CubeStateIndex_NumStates
};

class CubeStateMachine;

const float TEETH_ANIM_LENGTH = 1.0f;

class CubeState : public State
{
public:
    CubeState() : mStateMachine(0) { }
    void setStateMachine(CubeStateMachine& csm);
    CubeStateMachine& getStateMachine();

protected:
    void paintTeeth(VidMode_BG0& vid, bool animate=false, bool reverseAnim=false);
    void paintLetters(VidMode_BG0 &vid, const AssetImage &font);

private:
    CubeStateMachine* mStateMachine;
};

#endif // CUBESTATE_H
