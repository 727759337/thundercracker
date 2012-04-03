#include "TitleGameState.h"
#include "ScoredGameState.h"
#include "EventID.h"
#include "EventData.h"
#include "Dictionary.h"
#include "GameStateMachine.h"
#include "WordGame.h"
#include "assets.gen.h"

unsigned TitleGameState::update(float dt, float stateTime)
{
    return GameStateIndex_Title;
}

unsigned TitleGameState::onEvent(unsigned eventID, const EventData& data)
{

    return GameStateIndex_Title;
}
