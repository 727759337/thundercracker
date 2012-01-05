#ifndef SCOREDGAMESTATE_H
#define SCOREDGAMESTATE_H

#include "State.h"


class ScoredGameState : public State
{
public:    
    virtual unsigned update(float dt, float stateTime);
    virtual unsigned onEvent(unsigned eventID, const EventData& data);

    static void createNewAnagram();

    static void onAudioEvent(unsigned eventID, const EventData& data);

};

#endif // SCOREDGAMESTATE_H
