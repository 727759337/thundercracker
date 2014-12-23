/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker launcher
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "applet/status.h"
#include "mainmenu.h"
#include "shared.h"
#include "assets.gen.h"
#include <sifteo.h>

using namespace Sifteo;

static const float kBatteryLevelLow = 0.25f;

template<typename T>
static void drawBattery(T &canvas, float batteryLevel, Int2 pos)
{
    unsigned numBatteryLevels = ceil(batteryLevel * float(BatteryBars.numFrames()));
    
    if (numBatteryLevels > 0) {
        --numBatteryLevels;
        ASSERT(numBatteryLevels < BatteryBars.numFrames());
        canvas.image(pos, BatteryBars, numBatteryLevels);
    } else {
        canvas.image(pos, BatteryBars, 0);
    }
}

static void drawText(MainMenuItem::IconBuffer &icon, const char* text, Int2 pos)
{
    for (int i = 0; text[i] != 0; i++) {
        icon.image(vec(pos.x + i, pos.y), Font, text[i]-32);
    }
}

static void drawGraph(MainMenuItem::IconBuffer &icon, unsigned percentage, Int2 pos)
{
    int pixelage = percentage * 80 / 100;

    for (int i = 0; pixelage > 0; i++) {
        icon.image(vec(pos.x + i, pos.y), BarGraph, pixelage >= 8 ? 0 : pixelage);
        pixelage -= 8;
    }
}

static unsigned getUsedMemory()
{
    FilesystemInfo info;
    info.gather();
    
    float percentage = float(info.totalUnits() - info.freeUnits()) / float(info.totalUnits());
    
    return unsigned(percentage * 100);
}

void StatusApplet::getAssets(Sifteo::MenuItem &assets, Shared::AssetConfiguration &config)
{
    drawIcon(CubeID());
    assets.icon = menuIcon;
}

void StatusApplet::exec()
{
}

void StatusApplet::arrive()
{
    ASSERT(menu);
    // The number of cubes connected, or the base and menu cube battery status
    // could have changed while the Status applet was inactive.
    drawIcon(menu->cube());
    menu->replaceIcon(menuItemIndex, menuIcon);

    // Draw Icon Background
    for (CubeID cube : CubeSet::connected()) {
        if (cube != menu->cube()) {
            drawCube(cube);
        }
    }

    // Low Battery SFX
    if (System::batteryLevel() <= kBatteryLevelLow) {
        AudioChannel(0).play(Sound_BatteryLowBase);
    } else {
        for (CubeID cube : CubeSet::connected()) {
            if (cube.batteryLevel() <= kBatteryLevelLow) {
                AudioChannel(0).play(Sound_BatteryLowCube);
                break;
            }
        }
    }
}

void StatusApplet::add(Array<MainMenuItem*, Shared::MAX_ITEMS> &items)
{
    static StatusApplet instance;
    items.append(&instance);
}

void StatusApplet::drawIcon(Sifteo::CubeID menuCube)
{
    menuIcon.init();
    menuIcon.image(vec(0,0), Icon_Status);

    if (menuCube.isDefined()) {
        drawBattery(menuIcon, menuCube.batteryLevel(), vec(8, 1));
    }
    drawBattery(menuIcon, System::batteryLevel(), vec(8, 7));
    
    unsigned numCubes = CubeSet::connected().count();
    ASSERT(numCubes < 100);

    String<8> bufferCubes;
    bufferCubes << numCubes;
    drawText(menuIcon, bufferCubes.c_str(), vec(3, 4));
    
    String<16> bufferBlocks;
    bufferBlocks << getUsedMemory();
    drawText(menuIcon, bufferBlocks.c_str(), vec(1, 9));
    drawGraph(menuIcon, getUsedMemory(), vec(1, 10));
}

void StatusApplet::drawCube(CubeID cube)
{
    auto &vid = Shared::video[cube];
    vid.initMode(BG0);
    vid.bg0.erase(Menu_StripeTile);
    vid.bg0.image(vec(2,2), Icon_StatusOther);
    drawBattery(vid.bg0, cube.batteryLevel(), vec(7, 9));
}

void StatusApplet::onCubeConnect(unsigned cid)
{
    ASSERT(menu);
    // Update menu icon
    drawIcon(menu->cube());
    menu->replaceIcon(menuItemIndex, menuIcon);
    drawCube(CubeID(cid));
}

void StatusApplet::onCubeDisconnect(unsigned cid)
{
    ASSERT(menu);
    // Update menu icon
    drawIcon(menu->cube());
    menu->replaceIcon(menuItemIndex, menuIcon);
}

void StatusApplet::onCubeBatteryLevelChange(unsigned cid)
{
    ASSERT(menu);
    // Update the appropriate cube's icon
    CubeID menuCube = menu->cube();
    if (cid == menuCube) {
        drawIcon(menuCube);
        menu->replaceIcon(menuItemIndex, menuIcon);
    } else {
        drawCube(CubeID(cid));
    }
}

void StatusApplet::onVolumeChanged(unsigned volumeHandle)
{
    ASSERT(menu);
    // Update menu icon
    drawIcon(menu->cube());
    menu->replaceIcon(menuItemIndex, menuIcon);
}
