/*
 * Thundercracker Launcher -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#pragma once
#include <sifteo.h>
#include "mainmenuitem.h"

class MainMenu;


/**
 * MainMenuItem subclass for menu items implemented by external ELF binaries.
 */
 
class ELFMainMenuItem : public MainMenuItem
{
public:

    virtual MainMenuItem::Flags getAssets(Sifteo::MenuItem &assets, Sifteo::MappedVolume &map);
    virtual void bootstrap(Sifteo::CubeSet cubes, ProgressDelegate &progress);

    virtual void exec() {
        volume.exec();
    }

    virtual CubeRange getCubeRange() const {
        return cubeRange;
    }

    /// Look for all games on the system, and add them to the MainMenu.
    static void findGames(MainMenu &menu);

    /// See if we can automatically execute a single game. (Simulator only)
    static void autoexec();

private:
    /**
     * Max number of ELF main menu items. This is mostly dictated by the system's
     * limit on number of AssetGroups per AssetSlot.
     */
    static const unsigned MAX_INSTANCES = _SYS_ASSET_GROUPS_PER_SLOT;

    /// How many asset slots can one app use?
    static const unsigned MAX_ASSET_SLOTS = _SYS_ASSET_SLOTS_PER_BANK;

    /// How big is an empty asset slot?
    static const unsigned TILES_PER_ASSET_SLOT = _SYS_TILES_PER_ASSETSLOT;

    /// Max number of bootstrap asset groups (Limited by max size of metadata values)
    static const unsigned MAX_BOOTSTRAP_GROUPS = _SYS_MAX_METADATA_ITEM_BYTES / sizeof(_SYSMetadataBootAsset);

    struct SlotInfo {
        unsigned totalBytes;
        unsigned totalTiles;
        unsigned uninstalledBytes;
        unsigned uninstalledTiles;
    };

    CubeRange cubeRange;
    uint8_t numAssetSlots;
    Sifteo::Volume volume;

    /*
     * Local storage for icon assets.
     *
     * The 'image' here stores an uncompressed copy, in RAM, of the icon's tile
     * indices. The 'group' references mapped AssetGroup data which isn't available
     * after we unmap the game's volume, but perhaps more importantly it stores
     * information about the load address of this icon's assets on each cube.
     */
    struct {
        Sifteo::RelocatableTileBuffer<12,12> image;
        Sifteo::AssetGroup group;
    } icon;

    /**
     * Initialize from a Volume. Returns 'true' if we can successfully create
     * an ELFMainMenuItem for the volume, or 'false' if it should not appear
     * on the main menu.
     */
    bool init(Sifteo::Volume volume);

    static ELFMainMenuItem instances[MAX_INSTANCES];
};
