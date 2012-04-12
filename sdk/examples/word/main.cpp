#include <sifteo.h>
#include "assets.gen.h"
#include "assets.gen.h"
#include "WordGame.h"
#include "EventID.h"
#include "EventData.h"
#include <sifteo/menu.h>

using namespace Sifteo;

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

static Metadata M = Metadata()
    .title("Word Caravan")
    .cubeRange(3,CubeID::NUM_SLOTS);

static const char* sideNames[] =
{
  "top", "left", "bottom", "right"  
};

static struct MenuItem gItems[] =
{ {&IconContinue, &LabelContinue},
  {&IconLocked1, &LabelLocked},
  {&IconLocked2, &LabelLocked},
  {&IconLocked3, &LabelLocked},
  {&IconLocked0, &LabelLocked},
  {&IconLocked1, &LabelLocked},
  {&IconLocked2, &LabelLocked},
  {&IconLocked3, &LabelLocked},
  {NULL, NULL} };

static struct MenuAssets gAssets =
{&BgTile, &Footer, &LabelEmpty, {&Tip0, &Tip1, &Tip2, NULL}};

void onCubeEventTouch(void *context, _SYSCubeID cid)
{
    LOG("cube event touch:\t%d\n", cid);
    // TODO TouchAndHold timer
    EventData data;
    data.mInput.mCubeID = cid;
    WordGame::onEvent(EventID_Touch, data);

#ifdef DEBUGzz
    LOG("cube event touch->shake, ID:\t%d\n", cid);
    EventData data;
    data.mInput.mCubeID = cid;
    WordGame::onEvent(EventID_Shake, data);
#endif
}

void onCubeEventShake(void *context, _SYSCubeID cid)
{
    LOG("cube event shake, ID:\t%d\n", cid);
    EventData data;
    data.mInput.mCubeID = cid;
    WordGame::onEvent(EventID_Shake, data);
}

void onCubeEventTilt(void *context, _SYSCubeID cid)
{
    LOG("cube event tilt:\t%d\n", cid);
    EventData data;
    data.mInput.mCubeID = cid;
    WordGame::onEvent(EventID_Tilt, data);
}

void onNeighborEventAdd(void *context,
    _SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    EventData data;
    WordGame::onEvent(EventID_AddNeighbor, data);
    LOG("neighbor add:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]);
}

void onNeighborEventRemove(void *context,
    _SYSCubeID c0, _SYSSideID s0, _SYSCubeID c1, _SYSSideID s1)
{
    EventData data;
    WordGame::onEvent(EventID_RemoveNeighbor, data);
    LOG("neighbor remove:\t%d/%s\t%d/%s\n", c0, sideNames[s0], c1, sideNames[s1]);
}

void accel(_SYSCubeID c)
{
    LOG("accelerometer changed\n");
}

void main()
{
    LOG("Hello, Word Caravan\n");

    _SYS_setVector(_SYS_CUBE_TOUCH, (void*) onCubeEventTouch, NULL);
    _SYS_setVector(_SYS_CUBE_SHAKE, (void*) onCubeEventShake, NULL);
    _SYS_setVector(_SYS_CUBE_TILT, (void*) onCubeEventTilt, NULL);
    _SYS_setVector(_SYS_NEIGHBOR_ADD, (void*) onNeighborEventAdd, NULL);
    _SYS_setVector(_SYS_NEIGHBOR_REMOVE, (void*) onNeighborEventRemove, NULL);

    static CubeID cubes[NUM_CUBES];
    static VideoBuffer vidBufs[NUM_CUBES];

    for (unsigned i = 0; i < arraysize(cubes); i++)
    {
        cubes[i].sys =  i;// + CUBE_ID_BASE;
        vidBufs[i].initMode(BG0_SPR_BG1);
        vidBufs[i].attach(cubes[i]);
    }

#ifdef OLD_LOAD
    if (1 && LOAD_ASSETS)
    {
        // start loading assets
        for (unsigned i = 0; i < arraysize(cubes); i++)
        {
            cubes[i].loadAssets(GameAssets);

            VideoBuffer rom(VideoMode::BG0_ROM);//cubes[i].vbuf);
            // TODO attach
            rom.init();

            rom.BG0_text(vec(1,1), "Loading...");
        }

        // wait for assets to finish loading
        for (;;)
        {
            bool done = true;

            for (unsigned i = 0; i < arraysize(cubes); i++)
            {
                VideoBuffer rom(VideoMode::BG0_ROM);//cubes[i].vbuf);
                // TODO attach
                rom.BG0_progressBar(vec(0,7),
                                    cubes[i].assetProgress(GameAssets,
                                                           VideoBuffer::LCD_width),
                                    2);

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
#endif // ifndef DEBUG

    // sync up
    /*
    WordGame::paintSync();
    for(CubeID p=cubes; p!=cubes+NUM_CUBES; ++p) { p->vbuf.touch(); }
    System::paintSync();
    for(CubeID p=cubes; p!=cubes+NUM_CUBES; ++p) { p->vbuf.touch(); }
    System::paintSync();
*/
    CubeID menuCube = cubes[0];
    Menu m(vidBufs[0], &gAssets, gItems);

    // main loop
    WordGame game(vidBufs, menuCube, m); // must not be static!

    TimeStep ts;
    ts.next();
    SystemTime lastPaint = ts.end();

    while (1)
    {
        ts.next();
        game.update(ts.delta());

        // decouple paint frequency from update frequency
        if (ts.end() - lastPaint >= 1.f/25.f)
        {
            game.onEvent(EventID_Paint, EventData());
            lastPaint = ts.end();
        }

        if (true || game.needsPaintSync()) // TODO can't seem to fix BG1 weirdness w/o this
        {
            game.paintSync();
        }
        else
        {
            System::paint(); // (will do nothing if screens haven't changed
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




