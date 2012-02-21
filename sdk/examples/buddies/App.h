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
///////////////////////////////////////////////////////////////////////////////////////////////////

class Puzzle;

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
    
    /// CubeWrapper Accessors
    const CubeWrapper &GetCubeWrapper(Sifteo::Cube::ID cubeId) const;
    CubeWrapper &GetCubeWrapper(Sifteo::Cube::ID cubeId);
    
    /// Event Notifications
    void OnNeighborAdd(
        Sifteo::Cube::ID cubeId0, Sifteo::Cube::Side cubeSide0,
        Sifteo::Cube::ID cubeId1, Sifteo::Cube::Side cubeSide1);
    void OnTilt(Sifteo::Cube::ID cubeId);
    void OnShake(Sifteo::Cube::ID cubeId);
    
private:
    void AddCube(Sifteo::Cube::ID cubeId);
    void RemoveCube(Sifteo::Cube::ID cubeId);
    void ResetCubesToPuzzle(const Puzzle &puzzle);
    
    void PlaySound();
    
    void StartGameState(GameState gameState);
    void UpdateGameState(float dt);
    void DrawGameState();
    
    void ShufflePieces();
    
    void ChooseHint();
    
    void UpdateSwap(float dt);
    void OnSwapBegin(unsigned int swapPiece0, unsigned int swapPiece1);
    void OnSwapExchange();
    void OnSwapFinish();
    
    // Cubes
    CubeWrapper mCubeWrappers[kNumCubes];
    
    // Audio
    Sifteo::AudioChannel mChannel;
    
    // State
    GameState mGameState;
    float mResetTimer;
    float mDelayTimer;
    
    // Input
    bool mTouching;
    
    // Scoring
    float mScoreTimer;
    unsigned int mScoreMoves;
    
    // Swapping
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
    
    // Hinting
    int mHintPieceSkip;
    int mHintPiece0;
    int mHintPiece1;
    float mHintBlinkTimer;
    bool mHintBlinking;
    
    // Shuffle Mode
    int mShuffleMoveCounter;
    bool mShufflePiecesMoved[NUM_SIDES * kNumCubes];
    float mShuffleHintTimer;
    
    // Story Mode
    unsigned int mPuzzleIndex;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
