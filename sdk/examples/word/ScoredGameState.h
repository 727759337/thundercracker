#ifndef SCOREDGAMESTATE_H
#define SCOREDGAMESTATE_H

#include "State.h"
#include "config.h"
#include "Constants.h"


class ScoredGameState : public State
{
public:    
    ScoredGameState();
    virtual unsigned update(float dt, float stateTime);
    virtual unsigned onEvent(unsigned eventID, const EventData& data);

    static void createNewAnagram();

    static void onAudioEvent(unsigned eventID, const EventData& data);

private:
    static void getRandomCubePermutation(unsigned char *indexArray);

    Cube::ID mHintCubeIDOnUpdate;
};

#endif // SCOREDGAMESTATE_H
