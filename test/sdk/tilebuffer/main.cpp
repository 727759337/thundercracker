#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static CubeID cube = 0;
static VideoBuffer vid;

static AssetSlot MainSlot = AssetSlot::allocate();

static Metadata M = Metadata()
    .title("TileBuffer test")
    .cubeRange(1);


static void testTileBuffer()
{
    /*
     * Relocated draw, from an asset group that's guaranteed not to
     * start at base address zero
     */
    
    vid.initMode(BG0_SPR_BG1);
    vid.attach(cube);

    {
        AssetConfiguration<2> config;
        ScopedAssetLoader loader;
        config.append(MainSlot, BootAssets);
        config.append(MainSlot, GameAssets);
        loader.start(config, CubeSet(cube));
    }

    TileBuffer<16, 16> tileBuffer(cube);
    tileBuffer.erase( Transparent );
    tileBuffer.image( vec(0,0), PointFont );
    vid.bg1.maskedImage( tileBuffer, Transparent, 0);

    System::paint();
    System::finish();

    SCRIPT(LUA, util:assertScreenshot(cube, 'test0'));
}

void main()
{
    // Bootstrapping that would normally be done by the Launcher
    while (!CubeSet::connected().test(cube))
        System::yield();
    _SYS_asset_bindSlots(_SYS_fs_runningVolume(), 1);

    AssetConfiguration<1> config;
    ScopedAssetLoader loader;
    SCRIPT(LUA, System():setAssetLoaderBypass(true));
    config.append(MainSlot, BootAssets);
    loader.start(config, CubeSet(cube));
    loader.finish();
    SCRIPT(LUA, System():setAssetLoaderBypass(false));

    SCRIPT(LUA,
        package.path = package.path .. ";../../lib/?.lua"
        require('siftulator')
        cube = Cube(0)
    );

    testTileBuffer();

    LOG("Success.\n");
}
