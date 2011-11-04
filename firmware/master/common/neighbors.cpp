/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <protocol.h>
#include <sifteo/machine.h>
#include "neighbors.h"
#include "vram.h"
#include "runtime.h"

#define CUBE_ID_MASK (0x1F)
#define HAS_NEIGHBOR_MASK (0x80)

using namespace Sifteo;

NeighborSlot NeighborSlot::instances[_SYS_NUM_CUBE_SLOTS];

struct NeighborPair {
    _SYSSideID side0;
    _SYSSideID side1;

    bool fullyConnected() const { return side0 >= 0 && side1 >= 0; }
    bool fullyDisconnected() const { return side0 < 0 && side1 < 0; }
    void clear() { side0=-1; side1=-1; }

    NeighborPair() : side0(-1), side1(-1) {}

    _SYSSideID setSideAndGetOtherSide(_SYSCubeID cid0, _SYSCubeID cid1, _SYSSideID side, NeighborPair** outPair) {
        // abstract the order-of-arguments invariant of lookup()
        if (cid0 < cid1) {
            *outPair = lookup(cid0, cid1);
            (*outPair)->side0 = side;
            return (*outPair)->side1;
        } else {
            *outPair = lookup(cid1, cid0);
            (*outPair)->side1 = side;
            return (*outPair)->side0;
        }
    }

    NeighborPair* lookup(_SYSCubeID cid0, _SYSCubeID cid1) {
        // invariant this == pairs[0]
        // invariant cid0 < cid1
        return (this + cid0 * (_SYS_NUM_CUBE_SLOTS-1) + (cid1-1));
    }
};

static NeighborPair gCubesToSides[(_SYS_NUM_CUBE_SLOTS-1)*(_SYS_NUM_CUBE_SLOTS-1)];

void NeighborSlot::computeEvents() {
    uint8_t rawNeighbors[4];
    CubeSlot::instances[id()].getRawNeighbors(rawNeighbors);
    for(_SYSSideID side=0; side<4; ++side) {
        if (prevNeighbors[side] & HAS_NEIGHBOR_MASK) {
            if (rawNeighbors[side] & HAS_NEIGHBOR_MASK) {
                if ((prevNeighbors[side] & CUBE_ID_MASK) != (rawNeighbors[side] & CUBE_ID_MASK)) {
                    // detected "switch" (addNeighborToSide will take care of removing the old one)
                    addNeighborToSide(rawNeighbors[side] & CUBE_ID_MASK, side);
                }
            } else {
                // detected remove
                removeNeighborFromSide(prevNeighbors[side] & CUBE_ID_MASK, side);
            }
        } else if (rawNeighbors[side] & HAS_NEIGHBOR_MASK) {
            // detected add
            addNeighborToSide(rawNeighbors[side] & CUBE_ID_MASK, side);
        }
    }    
    prevNeighbors[0] = rawNeighbors[0];
    prevNeighbors[1] = rawNeighbors[1];
    prevNeighbors[2] = rawNeighbors[2];
    prevNeighbors[3] = rawNeighbors[3];
}

void NeighborSlot::resetSlots(_SYSCubeIDVector cv) {
    while (cv) {
        _SYSCubeID cubeId = Intrinsic::CLZ(cv);
        memset(instances[cubeId].neighbors.sides, 0xff, sizeof instances[cubeId].neighbors);
        memset(instances[cubeId].prevNeighbors, 0x00, sizeof instances[cubeId].prevNeighbors);
        cv ^= Intrinsic::LZ(cubeId);
    }
}

void NeighborSlot::resetPairs(_SYSCubeIDVector cv) {
    while(cv) {
        _SYSCubeID cubeId = Intrinsic::CLZ(cv);
        for(_SYSCubeID i=0; i<cubeId; ++i) { 
            gCubesToSides->lookup(i, cubeId)->clear();
        }
        for(_SYSCubeID i=cubeId+1; i<_SYS_NUM_CUBE_SLOTS; ++i) {
            gCubesToSides->lookup(i, cubeId)->clear();
        }
        cv ^= Intrinsic::LZ(cubeId);
    }
}

void NeighborSlot::addNeighborToSide(_SYSCubeID dstId, _SYSSideID side) {
    NeighborPair* pair;
    _SYSSideID dstSide = gCubesToSides->setSideAndGetOtherSide(id(), dstId, side, &pair);
    if (pair->fullyConnected() && neighbors.sides[side] != dstId) {
        clearSide(side);
        instances[dstId].clearSide(dstSide);
        neighbors.sides[side] = dstId;
        instances[dstId].neighbors.sides[dstSide] = id();
        if (_SYS_vectors.neighborEvents.add) {
            _SYS_vectors.neighborEvents.add(id(), side, dstId, dstSide);
        }
    }
}

void NeighborSlot::clearSide(_SYSSideID side) {
    _SYSCubeID otherId = neighbors.sides[side];
    if (otherId != 0xff) {
        _SYSSideID otherSide = 0;
        while(instances[otherId].neighbors.sides[otherSide] != id()) { ++otherSide; }
        if (_SYS_vectors.neighborEvents.remove) {
            _SYS_vectors.neighborEvents.remove(id(), side, otherId, otherSide);
        }
        neighbors.sides[side] = 0xff;
        instances[otherId].neighbors.sides[otherSide] = 0xff;
    }    
}

void NeighborSlot::removeNeighborFromSide(_SYSCubeID dstId, _SYSSideID side) {
    NeighborPair* pair;
    gCubesToSides->setSideAndGetOtherSide(id(), dstId, -1, &pair);
    if (pair->fullyDisconnected() && neighbors.sides[side] == dstId) {
        clearSide(side);
    }
}
