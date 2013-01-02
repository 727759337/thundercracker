#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

static Metadata M = Metadata()
    .title("Trackycat~")
    .package("com.sifteo.extras.trackycat", "1.0")
    .cubeRange(1);

static const AssetTracker *mods[] = { &Bubbles, &Vox, &Jungle1, &Guitar, &Second, &Dizzy, &Unreal };
static unsigned current = 0;
static AudioTracker tracker;

void onTouch(void * unused, unsigned cube)
{
    if (CubeID(cube).isTouching()) {
        current = (current + 1) % arraysize(mods);
        tracker.stop();
        tracker.play(*mods[current]);
        // // Seek to beginning of song.
        // tracker.setPosition(0, 0);
    }
}

void main()
{
    const CubeID cube(0);
    static VideoBuffer vid;
    int tempoModifier = 0;

    tracker.play(*mods[current]);
    Events::cubeTouch.set(onTouch);

    vid.initMode(BG0);
    vid.attach(cube);

    while (1) {
        // Update tilt
        int xTilt = (cube.accel().xy()).x;
        if (abs(xTilt - tempoModifier) > 2 && xTilt >= -100) {
            tracker.setTempoModifier(xTilt);
            tempoModifier = xTilt;
            LOG("tempo modifier: %d%%\n", tempoModifier);
        }

        unsigned frame = SystemTime::now().cycleFrame(0.5, Cat.numFrames());
        vid.bg0.image(vec(0,0), Cat, frame);
        System::paint();
    }
}

