/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "game.h"

AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(BootAssets);

static Metadata M = Metadata()
    .title("Membrane")
    .icon(Icon)
    .cubeRange(NUM_CUBES);

void main()
{
    static Game game;

    game.title();
    game.init();
    game.run();
}
