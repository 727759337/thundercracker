#include <sifteo.h>
#include <stdlib.h>
#include "assets.gen.h"
#include "WordGame.h"
#include "EventID.h"

using namespace Sifteo;

static const char* sideNames[] =
{
  "top", "left", "bottom", "right"  
};

void neighbor_add(_SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    LOG(("neighbor add:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]));
}

void neighbor_remove(_SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    LOG(("neighbor remove:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]));
}

void accel(_SYSCubeID c)
{
    LOG(("accelerometer changed\n"));
}

void siftmain()
{
    LOG(("HELLO, WORLD"));
    //_SYS_vectors.cubeEvents.accelChange = accel;
    _SYS_vectors.neighborEvents.add = neighbor_add;
    _SYS_vectors.neighborEvents.remove = neighbor_remove;
    
    static Cube cubes[MAX_CUBES];

    //static CubeSim demos[] = { CubeSim(cubes[0]), CubeSim(cubes[1]) };
    
    // start loading assets
    for (unsigned i = 0; i < arraysize(cubes); i++)
    {
        cubes[i].enable(i);
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
            rom.BG0_progressBar(Vec2(0,7), cubes[i].assetProgress(GameAssets, VidMode_BG0::LCD_width), 2);
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

    /*
    for (unsigned i = 0; i < arraysize(demos); i++)
        demos[i].init();
    */

    // main loop
    static WordGame game((Cube*)cubes);
    float lastTime = System::clock();
    while (1)
    {
        float now = System::clock();
        float dt = now - lastTime;
        lastTime = now;

        game.update(dt);
        game.onEvent(EventID_Paint); // TODO decouple
        
        System::paint();
    }
}
