/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include <sifteo/abi.h>
#include "event.h"
#include "cube.h"
#include "neighborslot.h"

uint32_t Event::pending;
Event::VectorInfo Event::vectors[_SYS_NUM_VECTORS];


void Event::clearVectors()
{
    memset(vectors, 0, sizeof vectors);
}


void Event::dispatch()
{
    /*
     * Find the next event that needs to be dispatched, if any, and
     * dispatch it. In order to avoid allocating an unbounded number of
     * user-mode stack frames, this function is limited to dispatching
     * at most one event at a time.
     */

    if (!SvmRuntime::canSendEvent())
        return;

    /*
     * Process events, by type
     */

    while (pending) {
        _SYSVectorID vid = (_SYSVectorID) Intrinsic::CLZ(pending);
        ASSERT(vid < _SYS_NUM_VECTORS);
        uint32_t vidMask = Intrinsic::LZ(vid);
        VectorInfo &vi = vectors[vid];

        // Base events carry a parameter instead of a Cube ID, the meaning of
        // which depends on the event type.
        if (vidMask & _SYS_BASE_EVENTS) {
            // Handle one normal Base event.
            // Clear the event bit first, so that on the next dispatch() we
            // immediately skip to the next pending event, if any.

            Atomic::And(pending, ~vidMask);

            if (callBaseEvent(vid, Atomic::Load(vi.param))) {
                return;
            }
            continue;
        }

        // Currently all cube events are dispatched per-cube, even the neighbor
        // events. Loop over the Cube IDs from cubesPending.
        while (vi.cubesPending) {
            _SYSCubeID cid = (_SYSCubeID) Intrinsic::CLZ(vi.cubesPending);

            // Type-specific event dispatch

            if (vidMask & _SYS_NEIGHBOR_EVENTS) {
                // Handle the first neighbor event for this cube slot.
                // Since there may be many events, we only mark the slot
                // as complete when sendNextEvent() returns false.

                if (NeighborSlot::instances[cid].sendNextEvent())
                    return;

                Atomic::And(vi.cubesPending, ~Intrinsic::LZ(cid));
                vidMask = _SYS_NEIGHBOR_EVENTS;

            } else {
                ASSERT(vidMask & _SYS_CUBE_EVENTS);
                // Handle one normal cube event.
                // We can clear the pending bit first, so that on the next
                // Dispatch() we immediately skip to the next pending event
                // if any.

                Atomic::And(vi.cubesPending, ~Intrinsic::LZ(cid));
                if (callCubeEvent(vid, cid))
                    return;
            }
        }

        // Once all cubesPending bits are clear, clear the vector bit.
        Atomic::And(pending, ~vidMask);
    }
}
