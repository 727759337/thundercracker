/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "ui_pause.h"
#include "svmloader.h"
#include "svmclock.h"
#include "event.h"

extern const uint16_t MenuBackground_data[];
extern const uint16_t IconQuit_data[];
extern const uint16_t IconBack_data[];
extern const uint16_t IconResume_data[];

char UIPause::gameMenuLabel[MAX_LABEL_CHARS + 1];

const UIMenu::Item UIPause::items[NUM_ITEMS] = {
    // Note: labels with an odd number of characters will center perfectly
    { IconBack_data,    gameMenuLabel},
    { IconResume_data,  "Continue Game" },
    { IconQuit_data,    "Quit Game" },
};


UIPause::UIPause(UICoordinator &uic)
    : menu(uic, &items[getFirstItem()], NUM_ITEMS - getFirstItem())
{}

void UIPause::init()
{
    ASSERT(SvmLoader::getRunLevel() != SvmLoader::RUNLEVEL_LAUNCHER);
    menu.init(ITEM_CONTINUE - getFirstItem());
}

void UIPause::takeAction()
{
    ASSERT(!SvmClock::isPaused());

    if (isDone()) {
        switch (menu.getChosenItem() + getFirstItem()) {

            case ITEM_GAME_MENU:
                Event::setBasePending(Event::PID_BASE_GAME_MENU);
                break;

            case ITEM_CONTINUE:
                break;

            case ITEM_QUIT:
                SvmLoader::exit();
                break;

            default:
                ASSERT(0);
                break;
        }
    }
}

bool UIPause::setGameMenuLabel(SvmMemory::VirtAddr label)
{
    FlashBlockRef ref;
    if (SvmMemory::strlcpyROData(ref, gameMenuLabel, label, sizeof gameMenuLabel))
         return true;

    // Memory fault; make sure we clear the label
    disableGameMenu();
    return false;
}

void UIPause::disableGameMenu()
{
    gameMenuLabel[0] = '\0';
    ASSERT(!hasGameMenu());
}
