/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * XXX: This is all just a temporary proof-of-concept now.
 */

#include <sifteo.h>
#include <sifteo/menu.h>
#include "assets.gen.h"
using namespace Sifteo;

#define BROKENORDINAL

#define NUM_CUBES 3
#define MAX_GAMES 24
VideoBuffer vid[CUBE_ALLOCATION];
static AssetSlot MainSlot = AssetSlot::allocate();

unsigned getMinCubes(MappedVolume &map)
{
    auto range = map.metadata<_SYSMetadataCubeRange>(_SYS_METADATA_CUBE_RANGE);
    return range ? range->minCubes : 0;
}

_SYSCubeIDVector getCubeVector(unsigned numCubes)
{
    return numCubes ? (0xFFFFFFFF << (32 - numCubes)) : 0;
}

void bootstrapAssetGroup(AssetGroup &group, AssetSlot &slot, unsigned numCubes)
{
    if (group.isInstalled(getCubeVector(numCubes))) {
        LOG("LAUNCHER: Bootstrap asset group %P already installed\n", group.sys.pHdr);
        return;
    }

    LOG("LAUNCHER: Installing bootstrap asset group %P in slot %d\n",
        group.sys.pHdr, slot.sys);

    ScopedAssetLoader loader;
    if (!loader.start(group, slot, getCubeVector(numCubes))) {
        // Out of space. Erase the slot first.
        LOG("LAUNCHER: Erasing asset slot\n");
        slot.erase();
        loader.start(group, slot, getCubeVector(numCubes));
    }

    for (CubeID cube = 0; cube != numCubes; ++cube) {
        vid[cube].initMode(BG0_ROM);
        vid[cube].bg0rom.text(vec(1,1), "Bootstrapping");
        vid[cube].bg0rom.text(vec(1,2), "game assets...");
        vid[cube].attach(cube);
    }

    while (!loader.isComplete()) {
        for (CubeID cube = 0; cube != numCubes; ++cube) {
            vid[cube].bg0rom.hBargraph(
                vec(0,7),
                loader.progress(cube, LCD_width),
                BG0ROMDrawable::ORANGE,
                2);
        }
        System::paint();
    }
}

void bootstrapMappedAssetGroup(MappedVolume &map, unsigned numCubes,
    const _SYSMetadataBootAsset &bootAsset)
{
    // Construct an AssetGroup on the stack
    AssetGroup group;
    map.writeAssetGroup(bootAsset, group);
    AssetSlot slot(bootAsset.slot);

    bootstrapAssetGroup(group, slot, numCubes);
}

void bootstrapAssets(MappedVolume &map, unsigned numCubes)
{
    /*
     * XXX: BIG HACK.
     *
     * Temporary code to load the bootstrap assets specified
     * in a game's metadata.
     *
     * In a real loader scenario, bootstrap assets would be loaded
     * onto a set of cubes determined by the loader: likely calculated
     * by looking at the game's supported cube range and the number of
     * connected cubes.
     *
     * In this hack, we just load the bootstrap assets onto the
     * first N cubes, where N is the minimum number required by the game.
     * If no cube range was specified, we don't load assets at all.
     */

    // Look up BootAsset array
    uint32_t actual;
    auto vec = map.metadata<_SYSMetadataBootAsset>(_SYS_METADATA_BOOT_ASSET, &actual);
    if (!vec) {
        LOG(("LAUNCHER: No bootstrap assets found\n"));
        return;
    }
    unsigned count = actual / sizeof *vec;

    if (!numCubes) {
        LOG(("LAUNCHER: Not loading bootstrap assets, no CubeRange\n"));
        return;
    }

    SCRIPT(LUA, System():setAssetLoaderBypass(true));

    for (unsigned i = 0; i < count; i++)
        bootstrapMappedAssetGroup(map, numCubes, vec[i]);

    SCRIPT(LUA, System():setAssetLoaderBypass(false));
}

void exec(Volume vol)
{
    MappedVolume map(vol);

    LOG("LAUNCHER: Running volume %08x\n", vol.sys);

    // Make way for the next ELF's assets.
    MainSlot.erase();

    // Enable the game's minimum set of cubes.
    unsigned numCubes = getMinCubes(map);
    SCRIPT_FMT(LUA, "System():setOptions{ numCubes = %d }", numCubes);
    _SYS_enableCubes(getCubeVector(numCubes));

    // Temporary asset bootstrapper
    bootstrapAssets(map, numCubes);

    vol.exec();
}

void main()
{
    SCRIPT(LUA, System():setAssetLoaderBypass(false));
    Array<Volume, MAX_GAMES> gGameList;
    Volume::list(Volume::T_GAME, gGameList);

    if (gGameList.empty()) {
        LOG("LAUNCHER: No games installed\n");
        while (1)
            System::paint();
    }

    if (gGameList.count() == 1) {
        exec(gGameList[0]);
    }

    static TileBuffer<96 / 8, 96 / 8> icons[MAX_GAMES];
    static struct MenuItem gItems[MAX_GAMES];

    // Self-bootstrapping
    _SYS_enableCubes(getCubeVector(NUM_CUBES));

    bootstrapAssetGroup(LauncherAssets, MainSlot, NUM_CUBES);

    for (unsigned i = 0; i < gGameList.count(); i++) {
        MappedVolume map(gGameList[i]);

        LOG("LAUNCHER: Found game vol=%08x, {%16h} \"%s\"\n",
            gGameList[i].sys, map.uuid()->bytes, map.title());

        auto metacon = map.metadata<_SYSMetadataImage>(_SYS_METADATA_ICON_96x96);
        if (metacon) {
#ifdef BROKENORDINAL
            gItems[i].icon = &NoIcon;
#else
            AssetImage icon;
            map.writeAssetImage(metacon, icon);

            // load the asset's group
            AssetGroup group;
            group.sys.pHdr = icon.sys.pAssetGroup;
            bootstrapAssetGroup(group, MainSlot, NUM_CUBES);
            icon.sys.pAssetGroup = reinterpret_cast<uint32_t>(&group.sys);

            icons[i].setCube(vid[0].cube());
            icons[i].image(vec(0,0), icon);
            gItems[i].icon = (AssetImage *)&icons[i];
#endif
        } else {
            gItems[i].icon = &NoIcon;
        }

        gItems[i].label = NULL;
    }
    gItems[gGameList.count()].icon = NULL;

    vid[0].attach(0);
    for(CubeID cube = 1; cube < NUM_CUBES; ++cube) {
        auto &v = vid[cube];
        v.initMode(BG0);
        v.bg0.erase(StripeTile);
        v.attach(cube);
    }

    static struct MenuAssets gAssets = {&BgTile, &Footer, NULL, {&Tip0, &Tip1, &Tip2, NULL}};

    Menu m(vid[0], &gAssets, gItems);
    m.setIconYOffset(8);

    struct MenuEvent e;
    while(m.pollEvent(&e)) {
        switch(e.type) {
            case MENU_ITEM_PRESS:
                exec(gGameList[e.item]);
            default:
                break;
        }
    }
}
