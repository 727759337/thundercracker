#include "Player.h"
#include "GameView.h"
#include "Game.h"
#include "Dialog.h"

#define DOOR_PAD 10
#define GAME_FRAMES_PER_ANIM_FRAME 2

inline int fast_abs(int x) {
	return x<0?-x:x;
}

Player::Player() : mStatus(PLAYER_STATUS_IDLE),
pCurrent(pGame->views), pTarget(0), 
mDir(2), mKeyCount(0), mItemMask(0), 
mAnimFrame(0), mAnimTime(0.f), mProgress(0), mNextDir(-1), 
mApproachingLockedDoor(false) {
  CORO_RESET;
}

void Player::Reset() {
  CORO_RESET;
  mPath.steps[0] = -1;
  mDir = 2;
  mProgress = 0;
  mNextDir = -1;
  mPosition = Vec2(64,64);
  mStatus = PLAYER_STATUS_IDLE;
  mApproachingLockedDoor = false;
}

Room* Player::CurrentRoom() const {
  return pGame->map.GetRoom(Location());
}

void Player::PickupItem(int itemId) {
  if (itemId == 0) { return; }
  PlaySfx(sfx_pickup);
  if (itemId == ITEM_BASIC_KEY || itemId == ITEM_SKELETON_KEY) {
    mKeyCount++;
    if (mKeyCount == 1) {
      pGame->OnInventoryChanged();
    }
  } else if (!HasItem(itemId)) {
    mItemMask |= (1<<itemId);
    ASSERT(HasItem(itemId));
    pGame->OnInventoryChanged();
  }
}

void Player::DecrementBasicKeyCount() { 
  ASSERT(mKeyCount>0); 
  mKeyCount--; 
  if (mKeyCount == 0) {
    pGame->OnInventoryChanged();
  }
}

void Player::SetLocation(Vec2 position, Cube::Side direction) {
  CORO_RESET;
  mPath.steps[0] = -1;
  mDir = direction;
  mProgress = 0;
  mNextDir = -1;
  mPosition = position;
  mStatus = PLAYER_STATUS_IDLE;
  mApproachingLockedDoor = false;
}

void Player::Move(int dx, int dy) {
  mStatus = PLAYER_STATUS_WALKING;
  mPosition.x += dx;
  mPosition.y += dy;
  mProgress += fast_abs(dx) + fast_abs(dy);
  mDir = InferDirection(Vec2(dx, dy));
}

int Player::CurrentFrame() {
  switch(mStatus) {
    case PLAYER_STATUS_IDLE: {
      if (mAnimFrame == 1) {
        return PlayerIdle.index;
      } else if (mAnimFrame == 2) {
        return PlayerIdle.index + 16;
      } else if (mAnimFrame == 3 || mAnimFrame == 4) {
        return PlayerIdle.index + (mAnimFrame-1) * 16;
      } else {
        return PlayerStand.index + SIDE_BOTTOM * 16;
      }
    }
    case PLAYER_STATUS_WALKING:
      int frame = mAnimFrame / GAME_FRAMES_PER_ANIM_FRAME;
      int tilesPerFrame = PlayerWalk.width * PlayerWalk.height;
      int tilesPerStrip = tilesPerFrame * (PlayerWalk.frames>>2);
      return PlayerWalk.index + mDir * tilesPerStrip + frame * tilesPerFrame;
  }
  return 0;
}

void Player::UpdateAnimation(float dt) {
  if (mStatus == PLAYER_STATUS_WALKING) {
    mAnimFrame = (mAnimFrame + 1) % (GAME_FRAMES_PER_ANIM_FRAME * (PlayerWalk.frames>>2));
  } else { // PLAYER_STATUS_IDLE
    mAnimTime += dt;
    if (mAnimTime >= 10.f) {
      mAnimTime -= 10.f;
    }
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
      pCurrent->UpdatePlayer();
    }
  }  
}

void Player::Update(float dt) {
  // every update code here
  UpdateAnimation(dt);

  CORO_BEGIN;
  // stately update code here
  for(;;) {
    // wait for tilt
    mStatus = PLAYER_STATUS_IDLE;
    pCurrent->UpdatePlayer();
    mNextDir = pCurrent->VirtualTiltDirection();
    mPath.Cancel();
    mAnimFrame = 0;
    mAnimTime = 0.f;
    while(!pGame->map.CanTraverse(pCurrent->Location(),mNextDir) || !(pTarget=pCurrent->VirtualNeighborAt(mNextDir))) {
      CORO_YIELD;
      mNextDir = pCurrent->VirtualTiltDirection();
      #if SIFTEO_SIMULATOR
      if (PathDetect()) { break; }
      #endif
    }
    // go to the target
    mStatus = PLAYER_STATUS_WALKING;
    mAnimFrame = 0;
    mDir = mNextDir;
    do {
      if (mPath.IsDefined()) {
        mDir = mPath.steps[0];
        pTarget = pCurrent->VirtualNeighborAt(mDir);
      }
      // animate walking to target
      PlaySfx(sfx_running);
      pTarget->ShowPlayer();
      if (mDir == SIDE_TOP && pCurrent->CurrentRoom()->HasClosedDoor()) {
        for(mProgress=0; mProgress<24; mProgress+=WALK_SPEED) {
          mPosition.y -= WALK_SPEED;
          pCurrent->UpdatePlayer();
          CORO_YIELD;
        }
        if (HaveBasicKey()) {
          DecrementBasicKeyCount();
          // check the door
          pCurrent->CurrentRoom()->OpenDoor();
          pCurrent->DrawBackground();
          pCurrent->GetCube()->vbuf.touch();
          pCurrent->UpdatePlayer();
          mTimeout = System::clock();
          pGame->NeedsSync();
          do {
            CORO_YIELD;
          } while(System::clock() - mTimeout <  0.5f);
          PlaySfx(sfx_doorOpen);
          // finish up
          for(; mProgress+WALK_SPEED<=128; mProgress+=WALK_SPEED) {
            mPosition.y -= WALK_SPEED;
            pCurrent->UpdatePlayer();
            pTarget->UpdatePlayer();
            CORO_YIELD;
          }
          // fill in the remainder
          mPosition = pTarget->CurrentRoom()->Center();
        } else {
          PlaySfx(sfx_doorBlock);
          mPath.Cancel();
          pTarget->HidePlayer();
          pTarget = 0;
          mDir = (mDir+2)%4;
          for(mProgress=0; mProgress<24; mProgress+=WALK_SPEED) {
            CORO_YIELD;
            mPosition.y += WALK_SPEED;
            pCurrent->UpdatePlayer();
          }          
        }
      } else { // general case - A*
		{
			bool result = pGame->map.FindPath(pCurrent->Location(), mDir, &mMoves);
			ASSERT(result);
		}
        mProgress = 0;
        for(pNextMove=mMoves.pFirstMove; pNextMove!=mMoves.End(); ++pNextMove) {
          mDir = *pNextMove;  
          if (mProgress != 0) {
            mPosition += mProgress * kSideToUnit[*pNextMove];
            pCurrent->UpdatePlayer();
            pTarget->UpdatePlayer();
            CORO_YIELD;
          }
          while(mProgress+WALK_SPEED < 16) {
            mProgress += WALK_SPEED;
            mPosition += WALK_SPEED * kSideToUnit[*pNextMove];
            pCurrent->UpdatePlayer();
            pTarget->UpdatePlayer();
            CORO_YIELD;
          }
          mPosition += (16 - mProgress) * kSideToUnit[*pNextMove];
          mProgress = WALK_SPEED - (16-mProgress);
        }
        if (mProgress != 0) {
          pNextMove--;
          mPosition += mProgress * kSideToUnit[*pNextMove];
          mProgress = 0;
          pCurrent->UpdatePlayer();
          pTarget->UpdatePlayer();
          CORO_YIELD;
        }
      }
      if (pTarget) { // did we land on the target?
        pCurrent->HidePlayer();
        pCurrent = pTarget;
        pTarget = 0;  
        mPosition = pCurrent->CurrentRoom()->Center();
        pCurrent->UpdatePlayer();        
        if (pCurrent->CurrentRoom()->HasItem()) {
          const ItemData* pItem = pCurrent->CurrentRoom()->TriggerAsItem();
          if (pGame->state.FlagTrigger(pItem->trigger)) { pCurrent->CurrentRoom()->ClearTrigger(); }
          PickupItem(pItem->itemId);
          // do a pickup animation
          for(unsigned frame=0; frame<PlayerPickup.frames; ++frame) {
            pCurrent->SetPlayerFrame(PlayerPickup.index + (frame * PlayerPickup.width * PlayerPickup.height));
            
            for(float t=System::clock(); System::clock()-t<0.075f;) {
              // this calc is kinda annoyingly complex
              float u = (System::clock() - t) / 0.075f;
              const float du = 1.f / (float) PlayerPickup.frames;
              u = (frame + u) * du;
              u = 1.f - (1.f-u)*(1.f-u)*(1.f-u)*(1.f-u);
              pGame->Paint();
              pCurrent->SetItemPosition(Vec2(0, -36.f * u) );
            }
          }
          pCurrent->SetPlayerFrame(PlayerStand.index+ SIDE_BOTTOM* PlayerStand.width * PlayerStand.height);
          for(float t=System::clock(); System::clock()-t<0.25f;) { System::paint(); }
          pCurrent->HideItem();        
        }
      }
    } while(mPath.PopStep(pCurrent));
    if (pCurrent->CurrentRoom()->HasGateway()) {
      const GatewayData* pGate = pCurrent->CurrentRoom()->TriggerAsGate();
      const MapData& targetMap = gMapData[pGate->targetMap];
      const GatewayData& pTargetGate = targetMap.gates[pGate->targetGate];
      if (pGame->state.FlagTrigger(pGate->trigger)) { pCurrent->CurrentRoom()->ClearTrigger(); } 
      pGame->WalkTo(128 * pCurrent->CurrentRoom()->Location() + Vec2(pGate->x, pGate->y));
      pGame->TeleportTo(gMapData[pGate->targetMap], Vec2(
        128 * (pTargetGate.trigger.room % targetMap.width) + pTargetGate.x,
        128 * (pTargetGate.trigger.room / targetMap.width) + pTargetGate.y
      ));
    } else if (pCurrent->CurrentRoom()->HasNPC()) {
      ////////
      mStatus = PLAYER_STATUS_IDLE;
      pCurrent->UpdatePlayer();
      for(int i=0; i<16; ++i) {
        pGame->Paint(true);
      }
      const NpcData* pNpc = pCurrent->CurrentRoom()->TriggerAsNPC();
      if (pGame->state.FlagTrigger(pNpc->trigger)) { pCurrent->CurrentRoom()->ClearTrigger(); }
      DoDialog(gDialogData[pNpc->dialog], pCurrent->GetCube());
      System::paintSync();
      pCurrent->Init();
      System::paintSync();
    }
    mDir = SIDE_BOTTOM;
    pCurrent->UpdatePlayer();
  }
  
  CORO_END;
}

//------------------------------------------------------------------------
// PATH DETECTION
//------------------------------------------------------------------------

Path::Path() {
  for(int i=0; i<NUM_CUBES-1; ++i) {
    steps[i] = -1;
  }
}

bool Path::PopStep(GameView* newRoot) {
  if (steps[0] == -1 || steps[1] == -1) {
    steps[0] = -1;
    return false;
  }
  for(int i=0; i<NUM_CUBES-2; ++i) {
    steps[i] = steps[i+1];
  }
  steps[NUM_CUBES-2] = -1;
  if (*steps != -1 && newRoot->VirtualNeighborAt(*steps)) {
    return true;
  }
  steps[0] = -1;
  return false;
}

void Path::Cancel() {
  steps[0] = -1;
}

bool Player::PathDetect() {
  if (pTarget) { return false; }
  for(GameView *p = pGame->ViewBegin(); p!=pGame->ViewEnd(); ++p) { p->visited = false; }
  pCurrent->visited = true;
  for(int side=0; side<NUM_SIDES; ++side) {
    mPath.steps[0] = side;
    if (pGame->map.CanTraverse(pCurrent->Location(), side) && PathVisit(pCurrent->VirtualNeighborAt(side), 1)) {
      return true;
    }
  }
  mPath.steps[0] = -1;
  return false;
}

bool Player::PathVisit(GameView* view, int depth) {
  if (!view || view->visited) { return false; }
  view->visited = true;
  if (view->VirtualTiltDirection() != -1) {
    mPath.steps[depth] = -1;
    return true;
  } else {
    for(int side=0; side<NUM_SIDES; ++side) {
      mPath.steps[depth] = side;
      if (pGame->map.CanTraverse(view->Location(), side) && PathVisit(view->VirtualNeighborAt(side), depth+1)) {
        return true;
      } else {
        mPath.steps[depth] = -1;
      }
    }
  }
  mPath.steps[depth] = -1;
  return false;
}


