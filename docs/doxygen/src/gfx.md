
Graphics Engine       {#gfx}
===============

# Overview

Sifteo cubes have a fairly unique graphics architecture. This section introduces the main differences you'll notice between Sifteo cubes and other systems you may be familiar with. We assume at least some basic familiarity with computer architecture and computer graphics.

## Display

![](@ref single-cube.png)

The display on each Sifteo Cube is the star of the show. It's what sets Sifteo Cubes apart from dominos, Mahjongg tiles, and most other objects people love to pick up and touch and play with.

Our displays are 128x128 pixels, with a color depth of 16 bits per pixel. This format is often called **RGB565**, since it uses 5 bits of information each to store the Red and Blue channels of each pixel, and 6 bits for Green. (The human eye is most sensitive to green, so we can certainly use an extra bit there!)

This RGB565 color representation is used pervasively throughout the Sifteo SDK and @ref asset_workflow "Asset Preparation" toolchain. To get the best quality results, we always recommend that you start with lossless true-color images. The asset tools will automatically perform various kinds of lossless and optionally lossy compression on these images, and any existing lossy compression will simply hinder these compression passes. Your images won't be looking their best, and they will take up more space! Yuck!

In particular, here are some tips for preparing graphics to use with the Sifteo SDK:

* __Never use JPEG__ images, or images that have been previously stored as JPEG. Always start with images in a lossless format like PNG or PSD.
* __Never dither__ your images! Dithered images are harder to compress, so they will take up more space and they won't look as good.
* Keep your graphics __big__. Each cube has a small display, and you shouldn't clutter it with too many things at once.
* Keep your graphics __vibrant__. The gamma curve of the display may change as the viewing angle changes during play, so be sure to use enough visual contrast.
* Use __animation and interactivity__ whenever you can. More interactivity equals more fun, and Sifteo Cubes are great at fluid animation.
* Use stir's __proof output__ or __siftulator__ if you want to see exactly how your images will look in-game.

## Distributed Rendering

Unlike most game systems, the display is not directly attached to the hardware your application runs on. For this reason, Sifteo cubes use a *distributed rendering* architecture:

![](@ref distributed-rendering.png)

Each cube has its own graphics engine hardware, display hardware, and storage. Cubes have two types of local memory:

* __Asset Flash__
   - It is relatively large (several megabytes).
   - It is very fast to draw from, but very slow to rewrite.
   - It contains uncompressed 16-bit pixel data, arranged in 8x8-pixel tiles.

* __Video RAM__
   - It is tiny (1 kilobyte).
   - It is fast both to read and to write.
   - It contains metadata and commands, typically no pixel data.
   - It orchestrates the process of drawing a scene or part of a scene to the display.
   - Applications keep a shadow copy of this memory in a Sifteo::VideoBuffer object.
   - The system continuously synchronizes each cube's Video RAM with any correspondingly attached Sifteo::VideoBuffer.

## Concurrency

This distributed architecture implies that many rendering operations are inherently **asynchronous**. The system synchronizes Video RAM with your Sifteo::VideoBuffer, the cube's graphics engine draws to the display, and the display refreshes its pixels. All of these processes run concurrently, and "in the background" with respect to the game code you write.

The system can automatically optimize performance by overlapping many common operations. Typically, the cube's graphics engine runs concurrently with a game's code, and when you're drawing to multiple cubes, those cubes can all update their displays concurrently.

In the SDK, this concurrency is managed at a very high level with the Sifteo::System::paint() and Sifteo::System::finish() API calls. In short, *paint* asks for some rendering to begin, and *finish* waits for it to complete. Unless your application has a specific reason to wait for the rendering to complete, however, it is not necessary to call *finish*.

Sifteo::System::paint() is backed by an internal *paint controller* which does a lot of work so you don't have to. Frame rate is automatically throttled. Even though each cube's graphics engine may internally be rendering at a different rate (as shown by the FPS display in Siftulator) your application sees a single frame rate, as measured by the rate at which *paint* calls complete. This rate will never be higher than 60 FPS or lower than 10 FPS, and usually it will match the frame rate of the slowest cube.

This is a simplified example timeline, showing one possible way that asynchronous rendering could occur:

![](@ref concurrency.png)

1. The system starts out idle
2. Game logic runs, and prepares the first frame for rendering.
3. Game code calls Sifteo::System::paint()
4. This begins asynchronously synchronizing each cube's Video RAM with the corresponding Sifteo::VideoBuffer. In the mean time, game logic begins working on the next frame.
5. After each cube receives its Sifteo::VideoBuffer changes, it begins rendering. This process uses data from Video RAM and from Asset Flash to compose a scene on the Display.
6. In this example, the Game Logic completes before the cubes are finished rendering. The system limits how far ahead the application can get, by blocking it until the previous frame is complete.
7. When all cubes finish rendering, the application is unblocked, and we begin painting the second frame. In this example, there were no changes to Cube 1's display on the second frame, so it remains idle.
8. Likewise, the third frame begins rendering on Cube 1 once the second frame is finished. In this example, the third frame *only* includes changes to Cube 1.

Note that reality is a little messier than this, since the system tries really hard to avoid blocking any one component unless it's absolutely necessary, and some latency is involved in communicating between the system and each cube. Because of this, no guarantees are made about exactly when Sifteo::System::paint() returns, only about the average rate at which *paint* calls complete. If you need hard guarantees that synchronization and rendering have finished on every cube, use Sifteo::System::finish().

## Graphics Modes

Due to the very limited amount of Video RAM available, the graphics engine defines several distinct *modes*, which each define a different behavior for the engine and potentially a different layout for the contents of Video RAM. These modes include different combinations of tiled layers and sprites, as well as low-resolution frame-buffer modes and a few additional special-purpose modes.

Each of these modes, however, fits into a consistent overall rendering framework that provides a few constants that you can rely on no matter which video mode you use:

![](@ref gfx-modes.png)

* __Paint control__ happens in the same way regardless of the active video mode. Some of the Video RAM is used for paint control.

* __Windowing__ is a feature in which only a portion of the display is actually repainted. The mode renderers operate on one horizontal scanline of pixels at a time, so even when windowing is in use the entire horizontal width of the display must be redrawn. Windowing can be used to create letterbox effects, to render status bars, dialogue, etc.

* __Rotation__ by a multiple of 90-degrees can be enabled as the very last step in the graphics pipeline, after all mode-specific drawing, and after windowing.

These effects can be composed over the course of multiple paint/finish operations. For example:

![](@ref rotated-windowing.png)

## Tiles

In order to make the most efficient use of the uncompressed pixel data in each cube's Asset Flash, these pixels are grouped into 8x8-pixel *tiles* which are de-duplicated during @ref asset_workflow "Asset Preparation". Any tile in Asset Flash may be uniquely identified by a 16-bit index. These indices are much smaller to store and transmit than the raw pixel data for an image.

This diagram illustrates how our tiling strategy helps games run more efficiently:

![](@ref tile-grid.png)

1. Asset images begin as lossless PNG files, stored on disk.
2. These PNGs are read in by *stir*, and chopped up into a grid of 8x8-pixel tiles.
3. Stir determines the smallest unique set of tiles that can represent all images in a particular Sifteo::AssetGroup. This step may optionally be lossy. (Stir can find or generate tiles that are "close enough" without being pixel-accurate.) These tiles are compressed further using a lossless codec, and included in your application's AssetGroup data.
4. The original asset is recreated as an array of indices into the AssetGroup's tiles. This data becomes a single Sifteo::AssetImage object. The index data can be much smaller than the original image, and duplicated tiles can be encoded in very little space.
5. At runtime, AssetGroups and AssetImages are loaded separately. The former are loaded (slowly) into Asset Flash, whereas the latter are used by application code to draw into Video RAM.

# Graphics Mode Reference

## BG0

This is the prototypical tile-based mode that many other modes are based on. The name is short for _background zero_, and you may find both the design and terminology familiar if you've ever worked on other tile-based video game systems. Many of the most popular 2D video game systems had hardware acceleration for one or more tile-based _background_ layers.

BG0 is both the simplest mode and the most efficient. It makes good use of the hardware's fast paths, and it is quite common for BG0 rendering rates to exceed the physical refresh rate of the LCD.

The Sifteo::BG0Drawable class understands the Video RAM layout used in BG0 mode. You can find an instance of this class as the *bg0* member inside Sifteo::VideoBuffer.

![](@ref bg0-layer.png)

In this mode there is a single layer, an infinitely-repeating 18x18 tile grid. Video RAM contains an array of 324 tile indices, each stored as a 16-bit integer. Under application control, the individual tiles in this grid can be freely defined, and the viewport may *pan* around the grid with single-pixel accuracy:

![](@ref bg0-viewport.png)

 If the panning coordinates are a multiple of 8 pixels, the BG0 tile grid is lined up with the edges of the display and you can see a 16x16 grid of whole tiles. If the panning coordinates are not a multiple of 8 pixels, the tiles on the borders of the display will be partially visible. Up to a 17x17 grid of (partial) tiles may be visible at any time. The 18th row/column can be used for advanced scrolling techniques that pre-load tile indices just before they pan into view. This so-called *infinite scrolling* technique can be used to implement menus, large side-scrolling maps, and so on.

## BG0_BG1

Just as you might create several composited layers in a photo manipulation or illustration tool in order to move objects independently, the Sifteo graphics engine supports a simple form of layer compositing.

Sometimes you only need a single layer. Since you can modify each tile index independently in BG0, any animation is possible as long as objects move in 8-pixel increments, or the entire BG0 layer can move as a single unit. But sometimes it's invaluable to have even a single object that "breaks the grid" and moves independently. In the **BG0_BG1** mode, a second _background one_ layer floats on top of BG0. You might use this for:

* Explanatory text that pops up over a scene
* Parallax scrolling, where differents parts of the scene scroll at different rates to simulate depth
* A scoreboard, overlaid on your game's playfield
* Large movable objects, like enemies or menu icons, which scroll against a static background

Like BG0, BG1 is defined as a grid of tile indices. You can combine any number of Asset Images when drawing BG1, as long as everything is aligned to the 8-pixel tile grid. But BG0 and BG1 use two independent grid systems; you can pan each layer independently. Additionally, BG1 supports 1-bit transparency. Pixels on BG1 which are at least 50% transparent will allow BG0 to show through. Note that Sifteo Cubes do not support alpha blending, so all pixels that are at least 50% opaque appear as fully opaque.

![](@ref bg1-stackup.png)

In this example, BG0 contains a side-scrolling level, and BG1 contains both a player avatar and a score meter. The avatar and score meter are stationary on the screen, so we don't need to make use of BG1's pixel-panning feature. By panning BG0, the level background can scroll left and give the appearance that the player is walking right. The tiles underlying the player sprite and the score meter can be independently replaced, in order to animate the player or change the score.

The amount of RAM available for BG1 is quite limited. Unfortunately, the Video RAM in each Sifteo Cube is too small to include two 18x18 grids as used by BG0. The space available to BG1 is only sufficient for a total of 144 tiles, or about 56% of the screen.

To overcome this limitation, BG1 is arranged as a 16x16 tile _virtual grid_ which can be backed by up to 144 tiles. This means that at least 112 tiles in this virtual grid must be left unallocated. Unallocated tiles always appear fully transparent.

Pixel panning, when applied to BG1, always describes the offset between the top-left corner of the virtual grid and the top-left corner of the display window. This means that the physical location of any tile depends both on the current pixel panning and on the tile's location in the virtual grid.

Tile allocation is managed by a __mask__ bitmap. Each tile in the virtual grid has a single corresponding bit in the mask. If the bit is 0, the tile is unallocated and fully transparent. If the bit is 1, that location on the virtual grid is assigned to the next available tile index in the 144-tile array. Array locations are always assigned by scanning the mask bitmap from left-to-right and top-to-bottom.

![](@ref bg1-layer.png)

The Sifteo::BG1Drawable class understands the Video RAM layout used for the BG1 layer in in **BG0_BG1** mode. You can find an instance of this class as the *bg1* member inside Sifteo::VideoBuffer.

This class provides several distinct methods to set up the mask and draw images to BG1. Depending on your application, it may be easiest to set up the mask by:

1. Allocating tiles one row at a time, using Sifteo::BG1Drawable::eraseMask() and Sifteo::BG1Drawable::fillMask().
2. Composing an entire mask at compile-time using Sifteo::BG1Mask, and applying it with Sifteo::BG1Drawable::setMask().
3. Modifying a Sifteo::BG1Mask at runtime, and applying it with Sifteo::BG1Drawable::setMask().

There are similarly multiple ways to draw the image data, after you've allocated tiles:

1. Use a raw _locationIndex_ to plot individual values on the 144-tile array, with Sifteo::BG1Drawable::plot() and Sifteo::BG1Drawable::span().
2. Use Sifteo::BG1Drawable::image() to draw a Sifteo::AssetImage, automatically _clipping_ it to the allocated subset of the virtual grid, and automatically calculating the proper _locationIndex_ for each tile.
3. Use Sifteo::BG1Drawable::maskedImage() to set the mask and draw an image in one step.
