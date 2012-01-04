#pragma once
#include "GameView.h"
#include "Player.h"
#include "Map.h"

#define NUM_ENEMIES 8

class Game {
public:
  GameView views[NUM_CUBES];
  Player player;
  Map map;
  float mSimTime;
  bool mNeedsSync;
  bool mIsDone;

public:

  // Getters
  
  GameView* ViewAt(int i) { return views+i; }
  GameView* ViewBegin() { return views; }
  GameView* ViewEnd() { return views+NUM_CUBES; }
  
  void MainLoop();
  
  // Trigger Actions
  void WalkTo(Vec2 position);
  void TeleportTo(const MapData& m, Vec2 position);
  float SimTime() const { return mSimTime; }

  void OnNeighborAdd(GameView* v1, Cube::Side s1, GameView* v2, Cube::Side s2);
  void OnNeighborRemove(GameView* v1, Cube::Side s1, GameView* v2, Cube::Side s2);
  
  void OnInventoryChanged();

  void NeedsSync() { mNeedsSync = true; }

private:

  float UpdateDeltaTime();

  void ObserveNeighbors(bool flag);
  void CheckMapNeighbors();
  void MovePlayerAndRedraw(int dx, int dy);
};

extern Game* pGame;

