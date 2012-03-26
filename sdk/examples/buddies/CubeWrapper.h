/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef SIFTEO_BUDDIES_CUBEWRAPPER_H_
#define SIFTEO_BUDDIES_CUBEWRAPPER_H_

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <sifteo/BG1Helper.h>
#include <sifteo/cube.h>
#include "BuddyId.h"
#include "GameState.h"
#include "Piece.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace Buddies {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CubeWrapper contains state for each Sifteo::Cube. Instead of exposing the Cube directly, this
/// class manages all access so it can enforce the game logic.
///////////////////////////////////////////////////////////////////////////////////////////////////

class CubeWrapper
{
public:
    CubeWrapper();
    
    void Reset();
    void Update(float dt);
    
    // Drawing
    void DrawClear();
    void DrawFlush();
    bool DrawNeedsSync();
    
    void DrawBuddy();
    
    void DrawBackground(const Sifteo::AssetImage &asset);
    void DrawBackgroundPartial(
        Sifteo::Int2 position,
        Sifteo::Int2 offset,
        Sifteo::Int2 size,
        const Sifteo::AssetImage &asset);
    void ScrollBackground(Sifteo::Int2 position);
    
    void DrawSprite(
        int spriteIndex,
        Sifteo::Int2 position,
        const Sifteo::PinnedAssetImage &asset, unsigned int assetFrame = 0);
    
    void DrawUiAsset(
        Sifteo::Int2 position,
        const Sifteo::AssetImage &asset, unsigned int assetFrame = 0);
    void DrawUiAssetPartial(
        Sifteo::Int2 position,
        Sifteo::Int2 offset,
        Sifteo::Int2 size,
        const Sifteo::AssetImage &asset, unsigned int assetFrame = 0);
    void DrawUiText(
        Sifteo::Int2 position,
        const Sifteo::AssetImage &assetFont,
        const char *text);
    void ScrollUi(Sifteo::Int2 position);
    
    // Asset Loading
    bool IsLoadingAssets();
    void LoadAssets();
    void DrawLoadingAssets();
    
    Sifteo::Cube::ID GetId();
    
    // Enable/Disable
    bool IsEnabled() const;
    void Enable(Sifteo::Cube::ID cubeId);
    void Disable();
    
    // Buddy
    BuddyId GetBuddyId() const;
    void SetBuddyId(BuddyId buddyId);
    
    // Pieces
    const Piece &GetPiece(Sifteo::Cube::Side side) const;
    void SetPiece(Sifteo::Cube::Side side, const Piece &piece);
    
    const Piece &GetPieceSolution(Sifteo::Cube::Side side) const;
    void SetPieceSolution(Sifteo::Cube::Side, const Piece &piece);
    
    Sifteo::Int2 GetPieceOffset(Sifteo::Cube::Side side) const;
    void SetPieceOffset(Sifteo::Cube::Side side, Sifteo::Int2 offset);
    
    void StartPieceBlinking(Sifteo::Cube::Side side);
    void StopPieceBlinking();
    
    // Tilt
    Sifteo::Cube::TiltState GetTiltState() const;
    Sifteo::Int2 GetAccelState() const;
    
    // State
    bool IsSolved() const;
    bool IsTouching() const;
    
private:
    Sifteo::VidMode_BG0_SPR_BG1 Video();
    
    void DrawPiece(const Piece &piece, Sifteo::Cube::Side side);
    
    Sifteo::Cube mCube;
    Sifteo::BG1Helper mBg1Helper;
    
    bool mEnabled;
    BuddyId mBuddyId;
    Piece mPieces[NUM_SIDES];
    Piece mPiecesSolution[NUM_SIDES];
    Sifteo::Int2 mPieceOffsets[NUM_SIDES];
    Sifteo::Cube::Side mPieceBlinking;
    float mPieceBlinkTimer;
    bool mPieceBlinkingOn;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#endif
