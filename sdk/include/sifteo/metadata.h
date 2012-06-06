/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the public interface to the Sifteo SDK.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#pragma once
#ifdef NOT_USERSPACE
#   error This is a userspace-only header, not allowed by the current build.
#endif

#include <sifteo/abi.h>

namespace Sifteo {


/**
 * @brief Metadata objects are special compile-time mechanisms for annotating your
 * game's ELF binary with additional data.
 *
 * Metadata can be added inside a function, as follows:
 *
 *     Metadata().title("My game");
 *
 * You can chain multiple kinds of metadata into one statement:
 *
 *     Metadata()
 *      .title("My game")
 *      .icon(MyIconAsset);
 *
 * You can also declare metadata as a global variable:
 *
 *     static Metadata M = Metadata()
 *      .title("My game")
 *      .icon(MyIconAsset);
 *
 * All metadata parameters must be known at compile-time to be constant.
 */
class Metadata {
public:
    /**
     * @brief Initialize all required system metadata. Other optional metadata can be
     * added using individual methods on the Metadata class.     
     */
    Metadata()
    {
        // Metadata that's automatically inserted only once
        if (_SYS_lti_counter("Sifteo.Metadata", 0) == 0) {

            // Count the total number of AssetGroupSlots in use
            unsigned numAGSlots = _SYS_lti_counter("Sifteo.AssetGroupSlot", -1);
            _SYS_lti_metadata(_SYS_METADATA_NUM_ASLOTS, "b", numAGSlots);

            // UUID for this particular build. Used by the system for asset caching.
            _SYS_lti_metadata(_SYS_METADATA_UUID, "IIII",
                _SYS_lti_uuid(0, 0), _SYS_lti_uuid(0, 1),
                _SYS_lti_uuid(0, 2), _SYS_lti_uuid(0, 3));
        }
    }

    /**
     * @brief Add a human-readable title string to this game's metadata.
     */
    Metadata &title(const char *str)
    {
        _SYS_lti_abort(_SYS_lti_counter("Sifteo.Metadata.Title", 0) != 0,
            "Duplicate Metadata::title() instance.");

        _SYS_lti_metadata(_SYS_METADATA_TITLE_STR, "sB", str, 0);

        return *this;
    }

    /**
     * @brief Add an icon image to this game's metadata. The image needs to be
     * 96x96 pixels, and it should reside in a separate AssetGroup.
     */
    Metadata &icon(const _SYSAssetImage &i)
    {
        _SYS_lti_abort(_SYS_lti_counter("Sifteo.Metadata.Icon", 0) != 0,
            "Duplicate Metadata::icon() instance.");
        _SYS_lti_abort(i.width != 96/8 || i.height != 96/8,
            "Metadata::icon() image must be 96x96 pixels in size.");

        return image(_SYS_METADATA_ICON_96x96, i);
    }

    /**
     * @brief Add an arbitrary image, as a _SYSMetadataImage item, with a
     * user-specified key.
     */
    Metadata &image(uint16_t key, const _SYSAssetImage &i)
    {
        // AssetGroup is in RAM, but we want the static initializer data
        _SYSAssetGroup *G = (_SYSAssetGroup*) _SYS_lti_initializer(
            reinterpret_cast<const void*>(i.pAssetGroup), true);

        // Build a _SYSMetadataImage struct
        _SYS_lti_metadata(key, "BBBBII",
            i.width, i.height, i.frames, i.format, G->pHdr, i.pData);

        return *this;
    }

    /**
     * @brief Set the minimum and maximum number of supported cubes for this game.
     */
    Metadata &cubeRange(unsigned minCubes, unsigned maxCubes)
    {
        _SYS_lti_abort(_SYS_lti_counter("Sifteo.Metadata.CubeRange", 0) != 0,
            "Duplicate Metadata::cubeRange() instance.");
        _SYS_lti_abort(minCubes > _SYS_NUM_CUBE_SLOTS,
            "Minimum number of cubes is too high.");
        _SYS_lti_abort(maxCubes > _SYS_NUM_CUBE_SLOTS,
            "Maximum number of cubes is too high.");
        _SYS_lti_abort(minCubes > maxCubes,
            "Minimum number of cubes must be <= maximum number");

        _SYS_lti_metadata(_SYS_METADATA_CUBE_RANGE, "BB", minCubes, maxCubes);

        return *this;
    }

    /**
     * @brief The one-argument version of cubeRange sets up identical minimum and
     * maximum cube counts. The game requires exactly this many cubes.
     */
    Metadata &cubeRange(unsigned count)
    {
        return cubeRange(count, count);
    }
};


}   // namespace Sifteo
