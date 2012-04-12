#include <sifteo.h>
#include "CubeState.h"
#include "CubeStateMachine.h"
#include "GameStateMachine.h"
#include "assets.gen.h"
#include "WordGame.h"
#include "TileTransparencyLookup.h"
#include "PartialAnimationData.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

using namespace Sifteo;

void CubeState::setStateMachine(CubeStateMachine& csm)
{
    mStateMachine = &csm;

    Cube& cube = csm.getCube();
    VidMode_BG0_SPR_BG1 vid(cube.vbuf);
    vid.init();
}

CubeStateMachine& CubeState::getStateMachine()
{
    ASSERT(mStateMachine != 0);
    return *mStateMachine;
}


unsigned CubeState::update(float dt, float stateTime)
{
    mEyeDirChangeDelay -= dt;
    mEyeDirChangeDelay = MAX(0.f, mEyeDirChangeDelay);
    mEyeBlinkDelay -= dt;
    mEyeBlinkDelay = MAX(0.f, mEyeBlinkDelay);

    return getStateMachine().getCurrentStateIndex();
}


