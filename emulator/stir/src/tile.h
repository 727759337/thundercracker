/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _TILE_H
#define _TILE_H

#include <stdint.h>
#include <memory>
#include <color.h>

class Tile;
class TileStack;
typedef std::shared_ptr<Tile> TileRef;


/*
 * Tile --
 *
 *    One fixed-size image tile, in palettized RGB565 color. 
 *    Tile objects are immutable after they are initially created.
 */

class Tile {
 public:
    Tile(uint8_t *rgba, size_t stride);

    static const unsigned SIZE = 8;       // Number of pixels on a side
    static const unsigned PIXELS = 64;    // Total pixels in a tile

    // Firmware chroma-key color
    static const uint16_t CHROMA_KEY = 0x4FF5;

    void render(uint8_t *rgba, size_t stride);

    bool usingChromaKey() {
	return mUsingChromaKey;
    }

    RGB565 pixel(unsigned i) {
	return mPixels[i];
    }

    RGB565 pixel(unsigned x, unsigned y) {
	return mPixels[x + y * SIZE];
    }

    double meanSquaredError(Tile &other);

 private:
    Tile() {}

    friend class TileStack;

    RGB565 mPixels[PIXELS];
    bool mUsingChromaKey;
};


/*
 * TileStack --
 *
 *    A stack of similar tiles, represented at any given time by a tile
 *    created via a per-pixel median operation on every tile in the set.
 *
 *    When the optimizer finds a tile that's similar to this set, it
 *    can add it to the set in order to statistically incorporate that
 *    tile's pixels into the median image we'll eventually generate
 *    for that set of tiles.
 */

class TileStack {
 public:
    void add(TileRef t);
    TileRef median();

 private:
    static const unsigned MAX_SIZE = 128;

    std::vector<TileRef> tiles;
    TileRef cache;
};


/*
 * TilePool --
 *
 *    An independent pool of tiles, supporting lossless or lossy optimization.
 */

class TilePool {
 public:
    TilePool(double _maxMSE)
        : maxMSE(_maxMSE) {}

    TileStack *add(TileRef t);
    TileStack *closest(TileRef t, double &mse);

 private:
    std::list<TileStack> sets;
    double maxMSE;
};


/*
 * TileGrid --
 *
 *    An image, converted into a matrix of TileStack references.
 */

class TileGrid {
 public:
    TileGrid(TilePool *pool);

    void load(uint8_t *rgba, size_t stride, unsigned width, unsigned height);
    bool load(const char *pngFilename);
    
    void render(uint8_t *rgba, size_t stride);

    unsigned width() {
	return mWidth;
    }

    unsigned height() {
	return mHeight;
    }

    TileStack *tile(unsigned x, unsigned y) {
	return tiles[x + y * mWidth];
    }

 private:
    TilePool *mPool;
    unsigned mWidth, mHeight;
    std::vector<TileStack*> tiles;
};

#endif
