/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "applet/getgames.h"
#include "mainmenu.h"
#include "assets.gen.h"
#include <sifteo.h>
using namespace Sifteo;


void GetGamesApplet::getAssets(Sifteo::MenuItem &assets, Shared::AssetConfiguration &config)
{
    assets.icon = &Icon_GetGames;
}

void GetGamesApplet::exec()
{
	// Anything to do here?
}

void GetGamesApplet::add(MainMenu &menu)
{
    static GetGamesApplet instance;
    menu.append(&instance);
}
