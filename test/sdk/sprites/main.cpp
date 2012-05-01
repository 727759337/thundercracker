#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static AssetSlot MainSlot = AssetSlot::allocate()
    .bootstrap(GameAssets);

static Metadata M = Metadata()
    .title("Sprites test")
    .cubeRange(1);

static const CubeID cube(0);
static VideoBuffer vid;

void main()
{
    SCRIPT(LUA,
        package.path = package.path .. ";../../lib/?.lua"
        require('siftulator')
        cube = Cube(0)
    );

    vid.initMode(BG0_SPR_BG1);
    vid.attach(cube);

    vid.bg0.image(vec(0,0), Background);

    // Allocate sprite IDs
    SpriteRef sBullet = vid.sprites[0];
    SpriteRef s1UP = vid.sprites[1];
    const unsigned num1UPs = vid.sprites.NUM_SPRITES - 1;

    // 1UPs
    for (unsigned i = 0; i < num1UPs; i++)
        s1UP[i].setImage(Sprite);

    // Bullet
    sBullet.setImage(Bullet);

    // BG1 Overlay
    const Int2 ovlPos = vid.bg1.tileSize() - Overlay.tileSize();
    vid.bg1.setMask(BG1Mask::filled(ovlPos, Overlay.tileSize()));
    vid.bg1.image(ovlPos, Overlay);

    SystemTime startTime = SystemTime::now();
    const int numFrames = 30;

    for (int frame = 0; frame < numFrames; frame++) {
        // Circle of 1UPs
        for (unsigned i = 0; i < num1UPs; i++) {
            float angle = frame * 0.02f + i * (M_PI*2 / num1UPs);
            s1UP[i].move(LCD_center - Sprite.pixelExtent() + polar(angle, 32.f));
        }

        // Scroll BG1
        vid.bg1.setPanning(frame * vec(-0.8f, 0.f));
        
        // Flying bullet
        sBullet.move(vec(130.f, 130.f) + frame * polar(0.05f, -60.f));

        System::paint();
        System::finish();
        SCRIPT_FMT(LUA, "util:assertScreenshot(cube, 'frame-%02d')", frame);
    }

    // Enforce minimum average frame rate
    float avgFPS = numFrames / float(SystemTime::now() - startTime);
    ASSERT(avgFPS > 10.f);
    ASSERT(avgFPS < 70.f);
}
