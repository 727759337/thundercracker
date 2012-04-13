#pragma once
#include "Common.h"
#include "Content.h"
#include "View.h"

struct ButterflyFriend {
    
    uint8_t active : 1;
    uint8_t dir : 3;
    uint8_t frame : 4;
    UByte2 pos;

    void Randomize();
    void Update();


};

// todo: remove

#define HOVER_COUNT 32
extern const int8_t kHoverTable[HOVER_COUNT];
