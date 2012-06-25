/*
 * Sifteo SDK Example.
 */

#include "loader.h"
#include "assets.gen.h"
using namespace Sifteo;

void MyLoader::load(AssetGroup &group, AssetSlot slot)
{
    // The bootstrap group should already be installed on all cubes.
    ASSERT(BootstrapGroup.isInstalled(cubes));

    // Early out if the group in question is already loaded
    if (group.isInstalled(cubes))
        return;

    // Draw a background image from the bootstrap asset group.
    for (CubeID cube : cubes) {
        vid[cube].initMode(BG0);
        vid[cube].attach(cube);

        /*
         * Must draw to the buffer after attaching it to a cube, so we
         * know how to relocate this image's tiles for the cube in question.
         */
        vid[cube].bg0.image(vec(0,0), LoadingBg);
    }

    // Start drawing the background
    System::paint();

    // Synchronously load the rest of the loading screen, if necessary
    assetLoader.start(LoadingGroup, loaderSlot, cubes);
    assetLoader.finish();

    // Immediately start asynchronously loading the group our caller requested
    assetLoader.start(group, slot, cubes);

    // Paint the rest of the loading screen
    System::finish();
    for (CubeID cube : cubes)
        vid[cube].bg0.image(vec(0,5), LoadingText);

    /*
     * Animate the loading screen itself appearing, with a vertical iris effect
     */

    for (unsigned height = 1; height < LoadingText.pixelHeight(); ++height) {
        System::finish();
        for (CubeID cube : cubes) {
            unsigned y = LCD_center.y - height / 2;
            vid[cube].bg0.setPanning(vec(0U, y));
            vid[cube].setWindow(y, height);
        }
        System::paint();
    }

    /*
     * Animate the rest of the loading process, using STAMP mode to draw
     * an animated progress bar.
     */

    for (CubeID cube : cubes) {
        System::finish();

        vid[cube].initMode(STAMP);
        vid[cube].setWindow(100, 10);
        vid[cube].stamp.disableKey();

        auto& fb = vid[cube].stamp.initFB<16, 16>();

        for (unsigned y = 0; y < 16; ++y)
            for (unsigned x = 0; x < 16; ++x)
                fb.plot(vec(x,y), (-x-y) & 0xF);
    }

    unsigned frame = 0;
    while (!assetLoader.isComplete()) {

        for (CubeID cube : cubes) {
            // Animate the horizontal window, to show progress
            vid[cube].stamp.setHWindow(0, assetLoader.progress(cube, LCD_width));

            // Animate the colormap at a steady rate
            const RGB565 bg = RGB565::fromRGB(0x444488);
            const RGB565 fg = RGB565::fromRGB(0xffffff);

            auto &cmap = vid[cube].colormap;
            cmap.fill(bg);
            for (unsigned i = 0; i < 8; i++)
                cmap[(frame + i) & 15] = bg.lerp(fg, i << 5);
        }

        // Throttle our frame rate, to load assets faster
        for (unsigned i = 0; i != 4; ++i)
            System::paint();

        frame++;
    }
}
