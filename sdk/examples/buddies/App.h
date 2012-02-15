/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef SIFTEO_BUDDIES_APP_H_
#define SIFTEO_BUDDIES_APP_H_

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <sifteo/audio.h>
#include "Config.h"
#include "CubeWrapper.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Buddies {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// App holds all the state necessary to run CubeBuddies. System events are forward directly
/// to this class. A single instance should live in main.
///////////////////////////////////////////////////////////////////////////////////////////////////

class App
{
public:
    App();
    
    void Init(); /// Called on program initialization
    void Reset(); /// Called after assets are loaded, or when game is reset
    void Update(float dt);
    void Draw();
    
    CubeWrapper &GetCubeWrapper(Sifteo::Cube::ID cubeId);
    
    // Event Notifications
    void OnNeighborAdd(Sifteo::Cube::ID cubeId0, Sifteo::Cube::Side cubeSide0, Sifteo::Cube::ID cubeId1, Sifteo::Cube::Side cubeSide1);
    void OnTilt(Sifteo::Cube::ID cubeId);
    void OnShake(Sifteo::Cube::ID cubeId);
    
private:
    void AddCube(Sifteo::Cube::ID cubeId);
    void RemoveCube(Sifteo::Cube::ID cubeId);
    void ResetCubes();
    
    void PlaySound();
    
    void StartShuffleState(ShuffleState shuffleState);
    void UpdateShuffle(float dt);
    void ShufflePieces();
    
    void StartAuthoredState(AuthoredState authoredState);
    void UpdateAuthored(float dt);
    
    void UpdateSwap(float dt);
    void OnSwapBegin(unsigned int swapPiece0, unsigned int swapPiece1);
    void OnSwapExchange();
    void OnSwapFinish();
    
    CubeWrapper mCubeWrappers[kNumCubes];
    Sifteo::AudioChannel mChannel;
    float mResetTimer;
    float mDelayTimer;
    
    enum SwapState
    {
        SWAP_STATE_NONE = 0,
        SWAP_STATE_OUT,
        SWAP_STATE_IN,
        
        NUM_SWAP_STATES
    };
    SwapState mSwapState;
    unsigned int mSwapPiece0;
    unsigned int mSwapPiece1;
    int mSwapAnimationCounter;
    
    ShuffleState mShuffleState;
    int mShuffleMoveCounter;
    float mShuffleScoreTime;
    bool mShufflePiecesMoved[NUM_SIDES * kNumCubes];
    
    AuthoredState mAuthoredState;
    unsigned int mAuthoredPuzzleIndex;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
