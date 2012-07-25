/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include <sifteo.h>
#include "mainmenu.h"
#include "elfmainmenuitem.h"
#include "assets.gen.h"
#include "applet/getgames.h"
#include "applet/status.h"
#include "shared.h"

using namespace Sifteo;

#ifndef SDK_VERSION
#error No SDK_VERSION is defined. Build system bug?
#endif

static Metadata M = Metadata()
    .title("System Launcher")
    .package("com.sifteo.launcher", TOSTRING(SDK_VERSION));

void main()
{
    // Wait a little bit to allow all cube connection events to process
    SystemTime time = SystemTime::now();
    while ((SystemTime::now() - time).milliseconds() < 500) {
        System::yield();
    }

    // In simulation, if exactly one game is installed, run it immediately.
    ELFMainMenuItem::autoexec();

    AudioTracker::play(Tracker_Startup);

    while (1) {
        static MainMenu menu;
        menu.init();

        // Populate the menu
        ELFMainMenuItem::findGames(menu);
        GetGamesApplet::add(menu);
        StatusApplet::add(menu);

        menu.run();
    }
}
