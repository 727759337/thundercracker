/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#pragma once
#include <sifteo.h>
#include <sifteo/menu.h>
#include "cuberange.h"

class ProgressDelegate;


/**
 * Abstract base class for things that are runnable from the main menu.
 * Includes its visual representation on the menu, as well as methods for
 * bootstrapping the game (if necessary) and for transferring control to it.
 */
 
class MainMenuItem
{
public:
    /**
     * Retrieve pointers to the AssetImages for this menu item. These may
     * be local items in our own binary, or they may be copied from a game.
     *
     * If this data has been copied from elsewhere, they will point to
     * a constructed AssetGroup that the caller must load before using these
     * assets.
     */
    virtual void getAssets(Sifteo::MenuItem &assets) = 0;

    /**
     * How many cubes are required by this menu item, if any?
     */
    virtual CubeRange getCubeRange() {
        return CubeRange();
    }

    /**
     * Bootstrap the assets necessary to launch this main menu.
     *
     * Uses the supplied ProgressDelegate to report progress during the load,
     * if a load is necessary. If no loading is necessary, returns immediately
     * without using the ProgressDelegate.
     *
     * This may switch the global AssetSlot bindings. The main menu must not
     * load any of its own assets between the calls to bootstrap() and exec().
     */
    virtual void bootstrap(Sifteo::CubeSet cubes, ProgressDelegate &progress) {}

    /**
     * Execute this menu item. This function may or may not return. If a
     * separate ELF binary is needed, we'll exec() that binary. Otherwise,
     * this performs some action and eventually returns.
     */
    virtual void exec() = 0;
};
