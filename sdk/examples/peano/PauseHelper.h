#pragma once

#include "Game.h"

namespace TotalsGame {

class PauseHelper {
    // coalescing all button presses together -- not totally correct,
    // but much simpler and efficient.

    float mSeconds;

public:

    PauseHelper()
    {
        mSeconds = 0;
    }

    void Update() {
        for(int i=0; i<Game::NUMBER_OF_CUBES; ++i) {
            if (Game::GetCube(i)->touching()) {
                mSeconds += Game::GetInstance().dt;
                return;
            }
        }
        mSeconds = 0;
    }

    void Reset() {
        mSeconds = 0;
    }

    bool IsTriggered()
    {
        return mSeconds > 1.0f;
    }

    //for placement new
    void* operator new (size_t size, void* ptr) throw() {return ptr;}
    void operator delete(void *ptr) {}

};



}

