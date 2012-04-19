#include "Game.h"

void Map::Init() {
  SetData(gMapData[gQuestData->mapId]);
}

Bomb* Map::BombFor(const ItemData& bomb) {
  for(Bomb* p=BombBegin(); p!=BombEnd(); ++p) {
    if (p->Item() == &bomb) {
      return p;
    }
  }
  return 0;
}


void Map::SetData(const MapData& map) { 
  if (mData != &map) {
    
    mBombCount = 0;
    mBlockCount = 0;
    mData = &map; 

    RefreshTriggers();
    
    if (map.sokoblocks) {
      for (const SokoblockData* p=map.sokoblocks; p->x; ++p) {
        mBlock[mBlockCount].Init(*p);
        mBlockCount++;
      }
      ASSERT(mBlockCount <= BLOCK_CAPACITY);
    }

    if (mData->trapdoors) {
      for(const TrapdoorData* p = mData->trapdoors; p->roomId!=p->respawnRoomId; ++p) {
        mRooms[p->roomId].SetTrapdoor(p);
      }
    }

    if (map.depots) {
      for(const DepotData* p=map.depots; p->room != 0xff; ++p) {
        mRooms[p->room].SetDepot(p);
      }
    }

    if (map.switches) {
      for(const SwitchData* p=map.switches; p->room != 0xff; ++p) {
        mRooms[p->room].SetSwitch(p);
      }
    }

    if (map.bombables) {
      // the sentinel for bombables is 127, not 255, because he only gets 7 bits :P
      for(const BombableData* p=map.bombables; p->rid != 0x7f; ++p) {
        if (p->orientation == BOMBABLE_ORIENTATION_HORIZONTAL) {
          mRooms[p->rid].SetCanBomb(RIGHT);
          mRooms[p->rid + 1].SetCanBomb(LEFT);
        } else {
          mRooms[p->rid].SetCanBomb(BOTTOM);
          mRooms[p->rid + mData->width].SetCanBomb(TOP);
        }
      }
    }
  }
}

static inline bool AtEnd(const TriggerData& trig) { return trig.eventType == 0xff; }

void Map::RefreshTriggers() {
  const Room* pEnd = mRooms + (mData->width*mData->height);
  for(Room* p=mRooms; p!=pEnd; ++p) { p->Clear(); }

  // find active triggers
  mBombCount = 0;
  if (mData->items) {
    for(const ItemData* p = mData->items; !AtEnd(p->trigger); ++p) {
      if (gGame.GetState().IsActive(p->trigger)) {
        mRooms[p->trigger.room].SetTrigger(TRIGGER_ITEM, &p->trigger);
        if (gItemTypeData[p->itemId].triggerType == ITEM_TRIGGER_BOMB) {
          ASSERT(mBombCount < BOMB_CAPACITY);
          mBomb[mBombCount].Initialize(p);
          mBombCount++;
        }
      }
    }
  }

  if (mData->gates) {
    for(const GatewayData* p = mData->gates; !AtEnd(p->trigger); ++p) {
      if (gGame.GetState().IsActive(p->trigger)) {
        mRooms[p->trigger.room].SetTrigger(TRIGGER_GATEWAY, &p->trigger);
      }
    }
  }
  
  if (mData->npcs) {
    for(const NpcData* p = mData->npcs; !AtEnd(p->trigger); ++p) {
      if (gGame.GetState().IsActive(p->trigger)) {
        mRooms[p->trigger.room].SetTrigger(TRIGGER_NPC, &p->trigger);
      }
    }
  }
  
  if (mData->diagonalSubdivisions) {
    for(const DiagonalSubdivisionData* p = mData->diagonalSubdivisions; p->roomId; ++p) {
      mRooms[p->roomId].SetDiagonalSubdivision(p);
    }
  }
  
  if (mData->bridgeSubdivisions) {
    for(const BridgeSubdivisionData* p = mData->bridgeSubdivisions; p->roomId; ++p) {
      mRooms[p->roomId].SetBridgeSubdivision(p);
    }
  }
  
  if (mData->doors) {
    for(const DoorData* p = mData->doors; !AtEnd(p->trigger); ++p) {
      // even deactivated doors are still set - they're just "open"
      mRooms[p->trigger.room].SetDoor(p);
    }
  }
  
  // walk the overlay RLE to find room breaks
  if (mData->rle_overlay) {
    const unsigned tend = mData->width * mData->height * 64; // 64 tiles per room
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

bool Map::IsVertexWalkable(Int2 vertex) {
  // convert global vertex into location / local vertex pair
  Int2 loc = vec(vertex.x>>3, vertex.y>>3);
  if (vertex.x == 0 || loc.x < 0 || loc.x >= mData->width || loc.y < 0 || loc.y >= mData->height) {
    return false; // out of bounds
  }
  vertex -= 8 * loc;
  return IsTileOpen(loc, vertex) && (
    vertex.x == 0 ? // are we between rooms?
      IsTileOpen(vec(loc.x-1, loc.y), vec(7, vertex.y)) : 
      IsTileOpen(loc, vec(vertex.x-1, vertex.y))
  );

}
