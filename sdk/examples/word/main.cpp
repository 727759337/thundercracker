#include <sifteo.h>
#include <stdlib.h>
#include "assets.gen.h"
#include "assets.gen.h"
#include "WordGame.h"
#include "EventID.h"
#include "EventData.h"

using namespace Sifteo;

static const char* sideNames[] =
{
  "top", "left", "bottom", "right"  
};

void onCubeEventTouch(_SYSCubeID cid)
{
    DEBUG_LOG(("cube event touch:\t%d\n", cid));
    WordGame::onEvent(EventID_Input, EventData());
}

void onCubeEventShake(_SYSCubeID cid)
{
    DEBUG_LOG(("cube event shake:\t%d\n", cid));
    WordGame::onEvent(EventID_Input, EventData());
}

void onCubeEventTilt(_SYSCubeID cid)
{
    DEBUG_LOG(("cube event tilt:\t%d\n", cid));
    WordGame::onEvent(EventID_Input, EventData());
}

void onNeighborEventAdd(_SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    EventData data;
    WordGame::onEvent(EventID_AddNeighbor, data);
    LOG(("neighbor add:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]));
}

void onNeighborEventRemove(_SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    EventData data;
    WordGame::onEvent(EventID_RemoveNeighbor, data);
    LOG(("neighbor remove:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]));
}

void accel(_SYSCubeID c)
{
    DEBUG_LOG(("accelerometer changed\n"));
}

static bool anyNeighbors(const Cube& c) {
    return c.hasPhysicalNeighborAt(0) ||
        c.hasPhysicalNeighborAt(1) ||
        c.hasPhysicalNeighborAt(2) ||
        c.hasPhysicalNeighborAt(3);
}

void siftmain()
{
    DEBUG_LOG(("Hello, Word Play 2\n"));

    _SYS_vectors.cubeEvents.touch = onCubeEventTouch;
    _SYS_vectors.cubeEvents.shake = onCubeEventShake;
#ifdef DEBUG_disable
    _SYS_vectors.cubeEvents.tilt = onCubeEventTilt;
#endif
    _SYS_vectors.neighborEvents.add = onNeighborEventAdd;
    _SYS_vectors.neighborEvents.remove = onNeighborEventRemove;

    static Cube cubes[NUM_CUBES]; // must be static!

    if (LOAD_ASSETS)
    {
        // start loading assets
        for (unsigned i = 0; i < arraysize(cubes); i++)
        {
            cubes[i].enable(i + CUBE_ID_BASE);
            cubes[i].loadAssets(GameAssets);

            VidMode_BG0_ROM rom(cubes[i].vbuf);
            rom.init();

            rom.BG0_text(Vec2(1,1), "Loading...");
        }

        // wait for assets to finish loading
        for (;;)
        {
            bool done = true;

            for (unsigned i = 0; i < arraysize(cubes); i++)
            {
                VidMode_BG0_ROM rom(cubes[i].vbuf);
                rom.BG0_progressBar(Vec2(0,7), cubes[i].assetProgress(GameAssets, VidMode_BG0_SPR_BG1::LCD_width), 2);
                if (!cubes[i].assetDone(GameAssets))
                {
                    done = false;
                }
            }

            System::paint();

            if (done)
            {
                break;
            }
        }
    }

    { // fake power-on
            for(unsigned hack=0; hack<4; ++hack) {
                for(unsigned i=0; i<NUM_CUBES; ++i) {
                    VidMode_BG0 mode(cubes[i].vbuf);
                    mode.init();
                    mode.BG0_drawAsset(Vec2(0,0), ScreenOff);
                    cubes[i].vbuf.touch();
                }
                System::paintSync();
            }
            unsigned cnt = 0;
            while(cnt < 2 * (NUM_CUBES-1)) {
                System::paint();
                cnt = 0;
                for(unsigned i=0; i<NUM_CUBES; ++i) {
                    for(Cube::Side s=0; s<4; ++s) {
                        cnt += cubes[i].hasPhysicalNeighborAt(s);
                    }
                    VidMode_BG0(cubes[i].vbuf)
                        .BG0_drawAsset(Vec2(0,0), anyNeighbors(cubes[i]) ? Title : ScreenOff);
                }
            }
        }

    // main loop
    WordGame game(cubes); // must not be static!
    float lastTime = System::clock();
    float lastPaint = System::clock();
    while (1)
    {
        float now = System::clock();
        float dt = now - lastTime;
        lastTime = now;

        game.update(dt);

        // decouple paint frequency from update frequency
        if (now - lastPaint >= 1.f/25.f)
        {
            game.onEvent(EventID_Paint, EventData());
            lastPaint = now;
        }

        if (game.needsPaintSync())
        {
            game.paintSync();
        }
        else
        {
            System::paint();
        }
    }
}

/*  The __cxa_pure_virtual function is an error handler that is invoked when a
    pure virtual function is called. If you are writing a C++ application that
    has pure virtual functions you must supply your own __cxa_pure_virtual
    error handler function. For example:
*/
extern "C" void __cxa_pure_virtual() { while (1); }



void assertWrapper(bool testResult)
{
#ifdef SIFTEO_SIMULATOR
    if (!testResult)
    {
        assert(testResult);
    }
#endif
}
