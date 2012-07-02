/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include <sifteo.h>
#include "mainmenu.h"
#include "elfmainmenuitem.h"
#include "assets.gen.h"

using namespace Sifteo;

static Metadata M = Metadata()
    .title("System Launcher")
    .package("com.sifteo.launcher", "0.1");


void main()
{
    // In simulation, if exactly one game is installed, run it immediately.
    ELFMainMenuItem::autoexec();

    AudioTracker::play(UISound_Startup);

    while (1) {
        static MainMenu menu;
        menu.init();
        ELFMainMenuItem::findGames(menu);
        menu.run();
    }
}
