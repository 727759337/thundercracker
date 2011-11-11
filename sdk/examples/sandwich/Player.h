#pragma once

#include "Base.h"
class GameView;

#define WALK_SPEED 4
#define PLAYER_STATUS_IDLE 0
#define PLAYER_STATUS_WALKING 1

struct Path {
  Cube::Side steps[NUM_CUBES-1];
  Path();
  bool IsDefined() const { return *steps >= 0; }
  bool PopStep(GameView* newRoot);
};

class Player {
private:
  CORO_PARAMS
  int mStatus;
  GameView* pCurrent;
  GameView* pTarget;
  Vec2 mPosition;
  int mDir;
  int mKeyCount;
  
  // stately variables
  int mProgress;
  int mNextDir;

  Path mPath;
  
public:
  Player();
  
  int CurrentFrame();
  GameView* CurrentView() { return pCurrent; }
  GameView* TargetView() { return pTarget; }
  GameView* KeyView() { return pTarget==0?pCurrent:pTarget; }
  Cube::Side Direction() { return mDir; }
  Vec2 Position() const { return mPosition; }
  Vec2 Room() const { return mPosition/128; }
  int Status() const { return mStatus; }

  void SetLocation(Vec2 position, Cube::Side direction);

  void Move(int dx, int dy);
  void Update(float dt);
  void Reset();
  
private:
  bool PathDetect();
  bool PathVisit(GameView* view, int depth);
};