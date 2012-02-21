/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "App.h"
#include <sifteo.h>
#include "Config.h"
#include "Puzzle.h"
#include "PuzzleData.h"
#include "assets.gen.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Buddies { namespace {

///////////////////////////////////////////////////////////////////////////////////////////////////
// || Various convenience functions...
// \/ 
///////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int GetNumMovedPieces(bool moved[], unsigned int num_pieces)
{
    unsigned int num_moved = 0;
    
    for (unsigned int i = 0; i < num_pieces; ++i)
    {
        if (moved[i])
        {
            ++num_moved;
        }
    }
    
    return num_moved;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int GetRandomNonMovedPiece(bool moved[], unsigned int num_moved)
{
    Random random;
    
    unsigned int pieceIndex = random.randrange(unsigned(num_moved));
    
    while (moved[pieceIndex])
    {
        pieceIndex = random.randrange(unsigned(num_moved));
    }
    
    return pieceIndex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int GetRandomOtherPiece(bool moved[], unsigned int num_moved, unsigned int notThisPiece)
{
    Random random;
    
    unsigned int pieceIndex = random.randrange(unsigned(num_moved));
    
    while (pieceIndex / NUM_SIDES == notThisPiece / NUM_SIDES)
    {
        pieceIndex = random.randrange(unsigned(num_moved));
    }
    
    return pieceIndex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool AnyTouching(App& app)
{
    for (unsigned int i = 0; i < kNumCubes; ++i)
    {
        if (app.GetCubeWrapper(i).IsEnabled() && app.GetCubeWrapper(i).IsTouching())
        {
            return true;
        }
    }
    
    return false;
}
                    
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool AllSolved(App& app)
{
    for (unsigned int i = 0; i < kNumCubes; ++i)
    {
        if (app.GetCubeWrapper(i).IsEnabled() && !app.GetCubeWrapper(i).IsSolved())
        {
            return false;
        }
    }
    
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Shuffle Mode
///////////////////////////////////////////////////////////////////////////////////////////////////

const char *kGameStateNames[NUM_GAME_STATES] =
{
    "SHUFFLE_STATE_NONE",
    "SHUFFLE_STATE_START",
    "GAME_STATE_SHUFFLE_SHAKE_TO_SCRAMBLE",
    "GAME_STATE_SHUFFLE_SCRAMBLING",
    "GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES",
    "GAME_STATE_SHUFFLE_PLAY",
    "GAME_STATE_SHUFFLE_SOLVED",
    "GAME_STATE_SHUFFLE_SCORE",
    "GAME_STATE_STORY_START",
    "GAME_STATE_STORY_INSTRUCTIONS",
    "GAME_STATE_STORY_PLAY",
    "GAME_STATE_STORY_SOLVED",
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

const int kSwapAnimationCount = 64 - 8; // Note: sprites are offset by 8 pixels by design

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

App::App()
    : mCubeWrappers()
    , mChannel()
    , mGameState(GAME_STATE_NONE)
    , mResetTimer(0.0f)
    , mDelayTimer(0.0f)
    , mTouching(false)
    , mSwapState(SWAP_STATE_NONE)
    , mSwapPiece0(0)
    , mSwapPiece1(0)
    , mSwapAnimationCounter(0)
    , mShuffleMoveCounter(0)
    , mShuffleScoreTime(0.0f)
    , mShuffleHintTimer(0.0f)
    , mShuffleHintPieceSkip(-1)
    , mShuffleHintPiece0(-1)
    , mShuffleHintPiece1(-1)
    , mPuzzleIndex(0)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::Init()
{
    for (unsigned int i = 0; i < kNumCubes; ++i)
    {
        AddCube(i);
    }
    
    //mChannel.init();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::Reset()
{
    switch (kGameMode)
    {
        default:
        case GAME_MODE_FREE_PLAY:
        {
            StartGameState(GAME_STATE_FREE_PLAY);
            break;
        }
        case GAME_MODE_SHUFFLE:
        {
            StartGameState(GAME_STATE_SHUFFLE_START);
            break;
        }
        case GAME_MODE_STORY:
        {
            StartGameState(GAME_STATE_STORY_START);
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::Update(float dt)
{
    UpdateGameState(dt);
    UpdateSwap(dt);
    
    // Cubes
    for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
    {
        if (mCubeWrappers[i].IsEnabled())
        {
            mCubeWrappers[i].Update(dt);
        }
    }
    
    // Reset Detection
    if (AnyTouching(*this))
    {
        mResetTimer -= dt;
        
        if (mResetTimer <= 0.0f)
        {
            mResetTimer = kResetTimerDuration;
            
            PlaySound();
            Reset();
        }
    }
    else
    {
        mResetTimer = kResetTimerDuration;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::Draw()
{
    DrawGameState();
    // TODO: paintSync() handling
    System::paint();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

CubeWrapper &App::GetCubeWrapper(Cube::ID cubeId)
{
    ASSERT(cubeId < arraysize(mCubeWrappers));
    
    return mCubeWrappers[cubeId];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::OnNeighborAdd(Cube::ID cubeId0, Cube::Side cubeSide0, Cube::ID cubeId1, Cube::Side cubeSide1)
{
    if (mGameState == GAME_STATE_STORY_CLUE)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_CLUE)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_1)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_2)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else
    {
        bool isSwapping = mSwapState != SWAP_STATE_NONE;
        
        bool isFixed =
            GetCubeWrapper(cubeId0).GetPiece(cubeSide0).mAttribute == Piece::ATTR_FIXED ||
            GetCubeWrapper(cubeId1).GetPiece(cubeSide1).mAttribute == Piece::ATTR_FIXED;
        
        bool isValidGameState =
            mGameState == GAME_STATE_FREE_PLAY ||
            mGameState == GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES ||
            mGameState == GAME_STATE_SHUFFLE_PLAY ||
            mGameState == GAME_STATE_STORY_PLAY;
        
        bool isValidCube =
            kGameMode != GAME_MODE_STORY ||
            (   cubeId0 < GetPuzzle(mPuzzleIndex).GetNumBuddies() &&
                cubeId1 < GetPuzzle(mPuzzleIndex).GetNumBuddies());
        
        if (!isSwapping && !isFixed && isValidGameState && isValidCube)
        {
            if (kGameMode == GAME_MODE_SHUFFLE && mGameState != GAME_STATE_SHUFFLE_PLAY)
            {
                StartGameState(GAME_STATE_SHUFFLE_PLAY);
            }
            else if (kGameMode == GAME_MODE_STORY && mGameState != GAME_STATE_STORY_PLAY)
            {
                StartGameState(GAME_STATE_STORY_PLAY);
            }
            
            OnSwapBegin(cubeId0 * NUM_SIDES + cubeSide0, cubeId1 * NUM_SIDES + cubeSide1);
        }
    }
    
    mShuffleHintTimer = kHintTimerOnDuration;
    mShuffleHintPiece0 = -1;
    mShuffleHintPiece1 = -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::OnTilt(Cube::ID cubeId)
{
    if(mGameState == GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES)
    {
        StartGameState(GAME_STATE_SHUFFLE_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_CLUE)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_1)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_2)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    
    mShuffleHintTimer = kHintTimerOnDuration;
    mShuffleHintPiece0 = -1;
    mShuffleHintPiece1 = -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::OnShake(Cube::ID cubeId)
{
    if( mGameState == GAME_STATE_SHUFFLE_SHAKE_TO_SCRAMBLE)
    {
        StartGameState(GAME_STATE_SHUFFLE_SCRAMBLING);
    }
    else if(mGameState == GAME_STATE_SHUFFLE_SCORE)
    {
        StartGameState(GAME_STATE_SHUFFLE_SCRAMBLING);
    }
    else if (mGameState == GAME_STATE_STORY_CLUE)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_1)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    else if (mGameState == GAME_STATE_STORY_HINT_2)
    {
        StartGameState(GAME_STATE_STORY_PLAY);
    }
    
    mShuffleHintTimer = kHintTimerOnDuration;
    mShuffleHintPiece0 = -1;
    mShuffleHintPiece1 = -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::AddCube(Cube::ID cubeId)
{
    ASSERT(cubeId < arraysize(mCubeWrappers));
    ASSERT(!mCubeWrappers[cubeId].IsEnabled());
    
    mCubeWrappers[cubeId].Enable(cubeId, cubeId % kMaxBuddies);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::RemoveCube(Cube::ID cubeId)
{
    ASSERT(cubeId < arraysize(mCubeWrappers));
    ASSERT(mCubeWrappers[cubeId].IsEnabled());
    
    mCubeWrappers[cubeId].Disable();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::PlaySound()
{
#ifdef SIFTEO_SIMULATOR
    mChannel.play(GemsSound);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::StartGameState(GameState gameState)
{
    mGameState = gameState;
    DEBUG_LOG(("State = %s\n", kGameStateNames[mGameState]));
    
    switch (mGameState)
    {
        case GAME_STATE_FREE_PLAY:
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].Reset();
                    
                    for (unsigned int j = 0; j < NUM_SIDES; ++j)
                    {
                        mCubeWrappers[i].SetPiece(j, GetPuzzleDefault().GetStartState(i, j));
                        mCubeWrappers[i].SetPieceSolution(j, GetPuzzleDefault().GetEndState(i, j));
                    }
                }
            }
        }
        case GAME_STATE_SHUFFLE_START:
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].Reset();
                    
                    for (unsigned int j = 0; j < NUM_SIDES; ++j)
                    {
                        mCubeWrappers[i].SetPiece(j, GetPuzzleDefault().GetStartState(i, j));
                        mCubeWrappers[i].SetPieceSolution(j, GetPuzzleDefault().GetEndState(i, j));
                    }
                }
            }
            mDelayTimer = kStateTimeDelayShort;
            mShuffleHintTimer = kHintTimerOnDuration;
            mShuffleHintPiece0 = -1;
            mShuffleHintPiece1 = -1;
            mTouching = false;
            break;
        }
        case GAME_STATE_SHUFFLE_SCRAMBLING:
        {
            mShuffleMoveCounter = 0;
            for (unsigned int i = 0; i < arraysize(mShufflePiecesMoved); ++i)
            {
                mShufflePiecesMoved[i] = false;
            }
            ShufflePieces();
            break;
        }
        case GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES:
        {
            mShuffleScoreTime = 0.0f;
            break;
        }
        case GAME_STATE_SHUFFLE_SOLVED:
        {
            mShuffleHintPiece0 = -1;
            mShuffleHintPiece1 = -1;
            break;
        }
        case GAME_STATE_STORY_START:
        {
            mPuzzleIndex = 0;
            StartGameState(GAME_STATE_STORY_CHAPTER_START);
            break;
        }
        case GAME_STATE_STORY_CHAPTER_START:
        {
            ASSERT(kNumCubes >= GetPuzzle(mPuzzleIndex).GetNumBuddies());
            for (unsigned int i = 0; i < GetPuzzle(mPuzzleIndex).GetNumBuddies(); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].Reset();
                    
                    for (unsigned int j = 0; j < NUM_SIDES; ++j)
                    {
                        ASSERT(mPuzzleIndex < GetNumPuzzles());
                        mCubeWrappers[i].SetPiece(j, GetPuzzle(mPuzzleIndex).GetStartState(i, j));
                        mCubeWrappers[i].SetPieceSolution(j, GetPuzzle(mPuzzleIndex).GetEndState(i, j));
                    }
                }
            }
            mDelayTimer = kStateTimeDelayLong;
            break;
        }
        case GAME_STATE_STORY_CUTSCENE_START:
        {
            mDelayTimer = kStateTimeDelayLong;
            break;
        }
        case GAME_STATE_STORY_DISPLAY_START_STATE:
        {
            mDelayTimer = kStateTimeDelayShort;
            break;
        }
        case GAME_STATE_STORY_PLAY:
        {
            ASSERT(kNumCubes >= GetPuzzle(mPuzzleIndex).GetNumBuddies());
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].ClearBg1();
                }
            }
            System::paintSync();
            break;
        }
        case GAME_STATE_STORY_HINT_1:
        {
            mDelayTimer = kStateTimeDelayLong;
            break;
        }
        case GAME_STATE_STORY_SOLVED:
        {
            mDelayTimer = kStateTimeDelayShort;
            break;
        }
        case GAME_STATE_STORY_CUTSCENE_END:
        {
            mDelayTimer = kStateTimeDelayLong;
            break;
        }
        case GAME_STATE_STORY_CHAPTER_END:
        {
            break;
        }
        default:
        {
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::UpdateGameState(float dt)
{
    switch (mGameState)
    {
        case GAME_STATE_SHUFFLE_START:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_SHUFFLE_SHAKE_TO_SCRAMBLE);
            }
            break;
        }
        case GAME_STATE_SHUFFLE_SCRAMBLING:
        {
            if (mSwapAnimationCounter == 0)
            {
                ASSERT(mDelayTimer > 0.0f);
                mDelayTimer -= dt;
                
                if (mDelayTimer <= 0.0f)
                {
                    mDelayTimer = 0.0f;
                    ShufflePieces();
                }
            }
            break;
        }
        case GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES:
        {
            mShuffleScoreTime += dt;
            
            if (AnyTouching(*this))
            {
                mTouching = true;
                StartGameState(GAME_STATE_SHUFFLE_PLAY);
            }
            
            break;
        }
        case GAME_STATE_SHUFFLE_PLAY:
        {
            mShuffleScoreTime += dt;
            
            if (mShuffleHintTimer > 0.0f)
            {
                mShuffleHintTimer -= dt;
                if (mShuffleHintTimer <= 0.0f)
                {
                    mShuffleHintTimer = 0.0f;
                    ChooseShuffleHint();
                }
            }
            
            if (mTouching)
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                    mShuffleHintTimer = kHintTimerOnDuration;
                    mShuffleHintPiece0 = -1;
                    mShuffleHintPiece1 = -1;

                }
            }
            else
            {
                if (AnyTouching(*this))
                {
                    mTouching = true;
                    ChooseShuffleHint();
                }
            }
            
            break;
        }
        case GAME_STATE_SHUFFLE_SOLVED:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_SHUFFLE_SCORE);
            }
            break;
        }
        case GAME_STATE_STORY_CHAPTER_START:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_CUTSCENE_START);
            }
            break;
        }
        case GAME_STATE_STORY_CUTSCENE_START:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_DISPLAY_START_STATE);
            }
            break;
        }
        case GAME_STATE_STORY_DISPLAY_START_STATE:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_CLUE);
            }
            break;
        }
        case GAME_STATE_STORY_CLUE:
        {
            if (!mTouching)
            {
                if (AnyTouching(*this))
                {
                    mTouching = true;
                    StartGameState(GAME_STATE_STORY_PLAY);
                }
            }
            else
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                }
            }
            
            break;
        }
        case GAME_STATE_STORY_PLAY:
        {
            if (!mTouching)
            {
                for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
                {
                    if (mCubeWrappers[i].IsEnabled() && mCubeWrappers[i].IsTouching())
                    {
                        mTouching = true;
                        StartGameState(GAME_STATE_STORY_HINT_1);
                        break;
                    }
                }
            }
            else
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                }
            }
            break;
        }
        case GAME_STATE_STORY_HINT_1:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_PLAY);
            }
            
            if (!mTouching)
            {
                for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
                {
                    if (mCubeWrappers[i].IsEnabled() && mCubeWrappers[i].IsTouching())
                    {
                        mTouching = true;
                        StartGameState(GAME_STATE_STORY_HINT_2);
                        break;
                    }
                }
            }
            else
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                }
            }
            break;
        }
        case GAME_STATE_STORY_HINT_2:
        {
            if (!mTouching)
            {
                for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
                {
                    if (mCubeWrappers[i].IsEnabled() && mCubeWrappers[i].IsTouching())
                    {
                        mTouching = true;
                        StartGameState(GAME_STATE_STORY_PLAY);
                        break;
                    }
                }
            }
            else
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                }
            }
            break;
        }
        case GAME_STATE_STORY_SOLVED:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_CUTSCENE_END);
            }
            break;
        }
        case GAME_STATE_STORY_CUTSCENE_END:
        {
            ASSERT(mDelayTimer > 0.0f);
            mDelayTimer -= dt;
            
            if (mDelayTimer <= 0.0f)
            {
                mDelayTimer = 0.0f;
                StartGameState(GAME_STATE_STORY_CHAPTER_END);
            }
            break;
        }
        case GAME_STATE_STORY_CHAPTER_END:
        {
            if (!mTouching)
            {
                for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
                {
                    if (mCubeWrappers[i].IsEnabled() && mCubeWrappers[i].IsTouching())
                    {
                        mTouching = true;
                        if (i == 0)
                        {
                            if (++mPuzzleIndex == GetNumPuzzles())
                            {
                                mPuzzleIndex = 0;
                            }
                            StartGameState(GAME_STATE_STORY_CHAPTER_START);
                        }
                        else if (i == 1)
                        {
                            StartGameState(GAME_STATE_STORY_CHAPTER_START);
                        }
                        else if (i == 2)
                        {
                            // TODO: go to main menu
                        }
                        break;
                    }
                }
            }
            else
            {
                if (!AnyTouching(*this))
                {
                    mTouching = false;
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::DrawGameState()
{
    if (kGameMode == GAME_MODE_FREE_PLAY)
    {
        for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
        {
            if (mCubeWrappers[i].IsEnabled())
            {
                mCubeWrappers[i].DrawBuddy();
            }
        }
    }
    else if (kGameMode == GAME_MODE_STORY)
    {
        if (mGameState == GAME_STATE_STORY_CHAPTER_START)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].DrawBackground(ChapterTitle);
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_CUTSCENE_START)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i == 0)
                    {
                        mCubeWrappers[i].DrawCutscene();
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_DISPLAY_START_STATE)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_CLUE)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                        mCubeWrappers[i].DrawClue(NULL);
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_PLAY)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_HINT_1)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                        
                        if (i == 0)
                        {
                            mCubeWrappers[i].DrawClue(NULL, false);
                        }
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_HINT_2)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                        
                        if (i == 0)
                        {
                            mCubeWrappers[i].DrawClue(NULL, true);
                        }
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_SOLVED)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i < GetPuzzle(mPuzzleIndex).GetNumBuddies())
                    {
                        mCubeWrappers[i].DrawBuddy();
                    }
                    else
                    {
                        mCubeWrappers[i].DrawBackground(ChapterTitle);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_CUTSCENE_END)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    if (i == 0)
                    {
                        mCubeWrappers[i].DrawCutscene();
                    }
                    else
                    {
                        mCubeWrappers[i].EnableBg0SprBg1Video();
                        mCubeWrappers[i].DrawBackground(ChapterSummary);
                    }
                }
            }
        }
        else if (mGameState == GAME_STATE_STORY_CHAPTER_END)
        {
            for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
            {
                if (mCubeWrappers[i].IsEnabled())
                {
                    mCubeWrappers[i].EnableBg0SprBg1Video();
                    
                    if (i == 0)
                    {
                        mCubeWrappers[i].DrawBackground(NextChapter);
                    }
                    else if (i == 1)
                    {
                        mCubeWrappers[i].DrawBackground(Retry);
                    }
                    else if (i == 2)
                    {
                        mCubeWrappers[i].DrawBackground(ExitToMainMenu);
                    }
                }
            }
        }
    }
    else if (kGameMode == GAME_MODE_SHUFFLE)
    {
        for (unsigned int i = 0; i < arraysize(mCubeWrappers); ++i)
        {
            if (mCubeWrappers[i].IsEnabled())
            {
                mCubeWrappers[i].DrawBuddy();
                mCubeWrappers[i].DrawShuffleUi(
                    mGameState,
                    mShuffleScoreTime,
                    mShuffleHintPiece0,
                    mShuffleHintPiece1);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::ShufflePieces()
{   
    DEBUG_LOG(("ShufflePiece... %d left\n", arraysize(mShufflePiecesMoved) - GetNumMovedPieces(mShufflePiecesMoved, arraysize(mShufflePiecesMoved))));
    
    ++mShuffleMoveCounter;
    
    unsigned int piece0 = GetRandomNonMovedPiece(mShufflePiecesMoved, arraysize(mShufflePiecesMoved));
    unsigned int piece1 = GetRandomOtherPiece(mShufflePiecesMoved, arraysize(mShufflePiecesMoved), piece0);
    
    OnSwapBegin(piece0, piece1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::ChooseShuffleHint()
{
    // Check all sides of all cubes against each other, looking for a perfect double swap!
    for (unsigned int iCube0 = 0; iCube0 < arraysize(mCubeWrappers); ++iCube0)
    {
        if (mCubeWrappers[iCube0].IsEnabled())
        {
            for (Cube::Side iSide0 = 0; iSide0 < NUM_SIDES; ++iSide0)
            {
                for (unsigned int iCube1 = 0; iCube1 < arraysize(mCubeWrappers); ++iCube1)
                {
                    if (mCubeWrappers[iCube1].IsEnabled() && iCube0 != iCube1)
                    {
                        for (Cube::Side iSide1 = 0; iSide1 < NUM_SIDES; ++iSide1)
                        {
                            const Piece &piece0 = mCubeWrappers[iCube0].GetPiece(iSide0);
                            const Piece &piece1 = mCubeWrappers[iCube1].GetPiece(iSide1);
                            
                            if (piece0.mMustSolve && piece0.mAttribute != Piece::ATTR_FIXED &&
                                piece1.mMustSolve && piece1.mAttribute != Piece::ATTR_FIXED)
                            {
                                const Piece &pieceSolution0 =
                                    mCubeWrappers[iCube0].GetPieceSolution(iSide0);
                                const Piece &pieceSolution1 =
                                    mCubeWrappers[iCube1].GetPieceSolution(iSide1);
                                
                                if (!Compare(piece0, pieceSolution0) &&
                                    !Compare(piece1, pieceSolution1))
                                {
                                    if (Compare(piece0, pieceSolution1) &&
                                        Compare(piece1, pieceSolution0))
                                    {
                                        mShuffleHintPiece0 = iCube0 * NUM_SIDES + iSide0;
                                        mShuffleHintPiece1 = iCube1 * NUM_SIDES + iSide1;
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // If there are no double swaps, look for a good single swap.
    for (unsigned int iCube0 = 0; iCube0 < arraysize(mCubeWrappers); ++iCube0)
    {
        if (mCubeWrappers[iCube0].IsEnabled())
        {
            for (Cube::Side iSide0 = 0; iSide0 < NUM_SIDES; ++iSide0)
            {
                const Piece &piece0 = mCubeWrappers[iCube0].GetPiece(iSide0);
                const Piece &pieceSolution0 = mCubeWrappers[iCube0].GetPieceSolution(iSide0);
                
                if (piece0.mAttribute != Piece::ATTR_FIXED && !Compare(piece0, pieceSolution0))
                {
                    for (unsigned int iCube1 = 0; iCube1 < arraysize(mCubeWrappers); ++iCube1)
                    {
                        if (mCubeWrappers[iCube1].IsEnabled() && iCube0 != iCube1)
                        {
                            for (Cube::Side iSide1 = 0; iSide1 < NUM_SIDES; ++iSide1)
                            {
                                if (mShuffleHintPieceSkip != int(iCube1 * NUM_SIDES + iSide1))
                                {
                                    const Piece &pieceSolution1 =
                                        mCubeWrappers[iCube1].GetPieceSolution(iSide1);
                                    
                                    if (Compare(piece0, pieceSolution1))
                                    {
                                        mShuffleHintPiece0 = iCube0 * NUM_SIDES + iSide0;
                                        mShuffleHintPiece1 = iCube1 * NUM_SIDES + iSide1;
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Clear our skip piece, see note below...
    mShuffleHintPieceSkip = -1;
    
    // If there are no single swaps, just move any piece that isn't positioned correctly.
    for (unsigned int iCube0 = 0; iCube0 < arraysize(mCubeWrappers); ++iCube0)
    {
        if (mCubeWrappers[iCube0].IsEnabled())
        {
            for (Cube::Side iSide0 = 0; iSide0 < NUM_SIDES; ++iSide0)
            {
                const Piece &piece0 = mCubeWrappers[iCube0].GetPiece(iSide0);
                const Piece &pieceSolution0 = mCubeWrappers[iCube0].GetPieceSolution(iSide0);
                
                if (piece0.mAttribute != Piece::ATTR_FIXED && !Compare(piece0, pieceSolution0))
                {
                    // Once we found a piece that's not in the correct spot, we know that there
                    // isn't a "right" place to swap it this turn (since the above loop failed)
                    // so just swap it anywhere. But make sure to remember where we swapped it
                    // so we don't just swap it back here next turn.
                    
                    Cube::ID iCube1 = (iCube0 + 1) % kNumCubes;
                    Cube::Side iSide1 = iSide0;
                    
                    mShuffleHintPiece0 = iCube0 * NUM_SIDES + iSide0;
                    mShuffleHintPiece1 = iCube1 * NUM_SIDES + iSide1;
                    
                    mShuffleHintPieceSkip = iCube1 * NUM_SIDES + iSide1;
                    return;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::UpdateSwap(float dt)
{
    if (mSwapState == SWAP_STATE_OUT)
    {
        ASSERT(mSwapAnimationCounter > 0);
        
        mSwapAnimationCounter -= kSwapAnimationSpeed;
        if (mSwapAnimationCounter < 0)
        {
            mSwapAnimationCounter = 0;
        }
        
        mCubeWrappers[mSwapPiece0 / NUM_SIDES].SetPieceOffset(mSwapPiece0 % NUM_SIDES, -kSwapAnimationCount + mSwapAnimationCounter);
        mCubeWrappers[mSwapPiece1 / NUM_SIDES].SetPieceOffset(mSwapPiece1 % NUM_SIDES, -kSwapAnimationCount + mSwapAnimationCounter);
        
        if (mSwapAnimationCounter == 0)
        {
            OnSwapExchange();
        }
    }
    else if (mSwapState == SWAP_STATE_IN)
    {
        ASSERT(mSwapAnimationCounter > 0);
        
        mSwapAnimationCounter -= kSwapAnimationSpeed;
        if (mSwapAnimationCounter < 0)
        {
            mSwapAnimationCounter = 0;
        }
        
        mCubeWrappers[mSwapPiece0 / NUM_SIDES].SetPieceOffset(mSwapPiece0 % NUM_SIDES, -mSwapAnimationCounter);
        mCubeWrappers[mSwapPiece1 / NUM_SIDES].SetPieceOffset(mSwapPiece1 % NUM_SIDES, -mSwapAnimationCounter);
        
        if (mSwapAnimationCounter == 0)
        {
            OnSwapFinish();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::OnSwapBegin(unsigned int swapPiece0, unsigned int swapPiece1)
{
    mSwapPiece0 = swapPiece0;
    mSwapPiece1 = swapPiece1;
    mSwapState = SWAP_STATE_OUT;
    mSwapAnimationCounter = kSwapAnimationCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////              

void App::OnSwapExchange()
{
    mSwapState = SWAP_STATE_IN;
    mSwapAnimationCounter = kSwapAnimationCount;
    
    Piece temp = mCubeWrappers[mSwapPiece0 / NUM_SIDES].GetPiece(mSwapPiece0 % NUM_SIDES);
    Piece piece0 = mCubeWrappers[mSwapPiece1 / NUM_SIDES].GetPiece(mSwapPiece1 % NUM_SIDES);
    Piece piece1 = temp;
    
    mCubeWrappers[mSwapPiece0 / NUM_SIDES].SetPiece(mSwapPiece0 % NUM_SIDES, piece0);
    mCubeWrappers[mSwapPiece1 / NUM_SIDES].SetPiece(mSwapPiece1 % NUM_SIDES, piece1);
    
    // Mark both as moved...
    mShufflePiecesMoved[mSwapPiece0] = true;
    mShufflePiecesMoved[mSwapPiece1] = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void App::OnSwapFinish()
{
    mSwapState = SWAP_STATE_NONE;
    
    if (mGameState == GAME_STATE_SHUFFLE_SCRAMBLING)
    {
        bool done = GetNumMovedPieces(mShufflePiecesMoved, arraysize(mShufflePiecesMoved)) == arraysize(mShufflePiecesMoved);
        if (kShuffleMaxMoves > 0)
        {
            done = done || mShuffleMoveCounter == kShuffleMaxMoves;
        }
    
        if (done)
        {
            StartGameState(GAME_STATE_SHUFFLE_UNSCRAMBLE_THE_FACES);
        }
        else
        {
            mDelayTimer += kShuffleScrambleTimerDelay;
        }
    }
    else if (mGameState == GAME_STATE_SHUFFLE_PLAY)
    {
        if (AllSolved(*this))
        {
            StartGameState(GAME_STATE_SHUFFLE_SOLVED);
            mDelayTimer = kStateTimeDelayShort;
        }
    }
    else if (mGameState == GAME_STATE_STORY_PLAY)
    {
        if (AllSolved(*this))
        {
            StartGameState(GAME_STATE_STORY_SOLVED);
            mDelayTimer = kStateTimeDelayShort;
        }
    }
    
    if (mCubeWrappers[mSwapPiece0 / NUM_SIDES].IsSolved() ||
        mCubeWrappers[mSwapPiece1 / NUM_SIDES].IsSolved())
    {
        PlaySound();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}
