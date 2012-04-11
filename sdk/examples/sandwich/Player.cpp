#include "Player.h"
#include "RoomView.h"
#include "Game.h"

#define DOOR_PAD 10
#define GAME_FRAMES_PER_ANIM_FRAME 2

inline int fast_abs(int x) {
	return x<0?-x:x;
}

void Player::Init(VideoBuffer* pPrimary) {
  const RoomData& room = gMapData[gQuestData->mapId].rooms[gQuestData->roomId];
  Viewport *pView = gGame.ViewAt(pPrimary->cube());
  mCurrent.view = (RoomView*)(pView);
  mCurrent.subdivision = 0;
  mTarget.view = 0;
  mPosition.x = 128 * (gQuestData->roomId % gMapData[gQuestData->mapId].width) + 16 * room.centerX;
  mPosition.y = 128 * (gQuestData->roomId / gMapData[gQuestData->mapId].width) + 16 * room.centerY;
  mStatus = PLAYER_STATUS_IDLE;
  mDir = 2;
  mAnimFrame = 0;
  mAnimTime = 0.f;
  mEquipment = 0;
}

Room* Player::GetRoom() const {
  return gGame.GetMap()->GetRoom(Location());
}

void Player::ConsumeEquipment() {
  ASSERT(mEquipment);
  gGame.GetState()->FlagTrigger(mEquipment->trigger);
  mEquipment = 0;
}

void Player::Move(int dx, int dy) { 
  SetStatus(PLAYER_STATUS_WALKING);
  mPosition.x += dx;
  mPosition.y += dy;
  if (mCurrent.view) { mCurrent.view->UpdatePlayer(); }
  if (mTarget.view) { mTarget.view->UpdatePlayer(); }
}

void Player::ClearTarget() {
  mTarget.view->HidePlayer();
  mTarget.view = 0;
}

void Player::AdvanceToTarget() {
  mCurrent.view->HidePlayer();
  mCurrent = mTarget;
  mTarget.view = 0;  
  mPosition = GetRoom()->Center(mCurrent.subdivision);
  mCurrent.view->UpdatePlayer();
}

int Player::AnimFrame() {
  switch(mStatus) {
    case PLAYER_STATUS_IDLE: {
      if (mAnimFrame == 1) {
        return PlayerIdle.tile(0);
      } else if (mAnimFrame == 2) {
        return PlayerIdle.tile(0) + 16;
      } else if (mAnimFrame == 3 || mAnimFrame == 4) {
        return PlayerIdle.tile(0) + (mAnimFrame-1) * 16;
      } else {
        return PlayerStand.tile(0) + BOTTOM * 16;
      }
    }
    case PLAYER_STATUS_WALKING:
      int frame = mAnimFrame / GAME_FRAMES_PER_ANIM_FRAME;
      int tilesPerFrame = PlayerWalk.numTilesPerFrame();
      int tilesPerStrip = tilesPerFrame * (PlayerWalk.numFrames()>>2);
      return PlayerWalk.tile(0) + mDir * tilesPerStrip + frame * tilesPerFrame;
  }
  return 0;
}

void Player::SetStatus(int status) {
  if (mStatus == status) { return; }
  mStatus = status;
  mAnimFrame = 0;
  mAnimTime = 0.f;
}

void Player::Update() {
  if (mStatus == PLAYER_STATUS_WALKING) {
    mAnimFrame = (mAnimFrame + 1) % (GAME_FRAMES_PER_ANIM_FRAME * (PlayerWalk.numFrames()>>2));
  } else { // PLAYER_STATUS_IDLE
    mAnimTime = fmod(mAnimTime+gGame.Dt().seconds(), 10.f);
    int before = mAnimFrame;
    if (mAnimTime > 0.5f && mAnimTime < 1.f) {
      mAnimFrame = 1;
    } else if (mAnimTime > 2.5f && mAnimTime < 3.f) {
      mAnimFrame = 2;
    } else if (mAnimTime > 5.f && mAnimTime < 5.5f) {
      mAnimFrame = (mAnimTime < 5.25f ? 3 : 4);
    } else {
      mAnimFrame = 0;
    }
    if (before != mAnimFrame) {
      mCurrent.view->UpdatePlayer();
    }
  }  
}
