/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "shared.h"
#include <sifteo.h>
using namespace Sifteo;


VideoBuffer Shared::video[CUBE_ALLOCATION];

AssetSlot Shared::primarySlot = AssetSlot::allocate();
AssetSlot Shared::secondarySlot = AssetSlot::allocate();
