#include "Map.h"
#include "Game.h"

void Map::Init() {
  SetData(gMapData[gQuestData->mapId]);
}

void Map::SetData(const MapData& map) { 
  if (mData != &map) {
    mData = &map; 

    const Room* pEnd = mRooms + (map.width*map.height);
    for(Room* p=mRooms; p!=pEnd; ++p) { p->Clear(); }

    // find active triggers
    for(const ItemData* p = mData->items; p!= mData->items + mData->itemCount; ++p) {
      if (pGame->GetState()->IsActive(p->trigger)) {
        ASSERT(!mRooms[p->trigger.room].HasTrigger());
        mRooms[p->trigger.room].SetTrigger(TRIGGER_ITEM, &p->trigger);
      }
    }
    for(const GatewayData* p = mData->gates; p != mData->gates + mData->gateCount; ++p) {
      if (pGame->GetState()->IsActive(p->trigger)) {
        ASSERT(!mRooms[p->trigger.room].HasTrigger());
        mRooms[p->trigger.room].SetTrigger(TRIGGER_GATEWAY, &p->trigger);
      }
    }
    for(const NpcData* p = mData->npcs; p != mData->npcs + mData->npcCount; ++p) {
      if (pGame->GetState()->IsActive(p->trigger)) {
        ASSERT(!mRooms[p->trigger.room].HasTrigger());
        mRooms[p->trigger.room].SetTrigger(TRIGGER_NPC, &p->trigger);
      }
    }
    for(const TrapdoorData* p = mData->trapdoors; p != mData->trapdoors + mData->trapdoorCount; ++p) {
      if (pGame->GetState()->IsActive(p->trigger)) {
        ASSERT(!mRooms[p->trigger.room].HasTrigger());
        mRooms[p->trigger.room].SetTrigger(TRIGGER_TRAPDOOR, &p->trigger);
      }
    }
    for(const DiagonalSubdivisionData* p = mData->diagonalSubdivisions; p != mData->diagonalSubdivisions+mData->diagonalSubdivisionCount; ++p) {
      mRooms[p->roomId].SetDiagonalSubdivision(p);
    }
    for(const BridgeSubdivisionData* p = mData->bridgeSubdivisions; p != mData->bridgeSubdivisions+mData->bridgeSubdivisionCount; ++p) {
      mRooms[p->roomId].SetBridgeSubdivision(p);
    }
    for(const DoorData* p = mData->doors; p != mData->doors + mData->doorCount; ++p) {
      mRooms[p->roomId].SetDoor(p);
    }
    // walk the overlay RLE to find room breaks
    if (mData->rle_overlay) {
      const unsigned tend = map.width * map.height * 64; // 64 tiles per room
      unsigned tid=0;
      int prevRoomId = -1;
      const uint8_t *p = mData->rle_overlay;
      while (tid < tend) {
        if (*p == 0xff) {
          tid += p[1];
          p+=2;
        } else {
          const int roomId = (tid >> 6);
          if (roomId > prevRoomId) {
            prevRoomId = roomId;
            mRooms[roomId].SetOverlay(p - mData->rle_overlay, tid - (roomId<<6));
          }
          p++;
          tid++;
        }
      }
    }
  }
}

bool Map::IsVertexWalkable(Vec2 vertex) {
  // convert global vertex into location / local vertex pair
  Vec2 loc = Vec2(vertex.x>>3, vertex.y>>3);
  if (vertex.x == 0 || loc.x < 0 || loc.x >= mData->width || loc.y < 0 || loc.y >= mData->height) {
    return false; // out of bounds
  }
  vertex -= 8 * loc;
  return IsTileOpen(loc, vertex) && (
    vertex.x == 0 ? // are we between rooms?
      IsTileOpen(Vec2(loc.x-1, loc.y), Vec2(7, vertex.y)) : 
      IsTileOpen(loc, Vec2(vertex.x-1, vertex.y))
  );

}
