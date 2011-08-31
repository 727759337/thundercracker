/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <float.h>
#include <algorithm>
#include <set>
#include "tile.h"
#include "lodepng.h"


Tile::Tile()
    : mUsingChromaKey(false)
    {}

Tile::Tile(bool usingChromaKey)
    : mUsingChromaKey(usingChromaKey)
    {}

Tile::Tile(uint8_t *rgba, size_t stride)
    : mUsingChromaKey(false)
{
    /*
     * Load the tile image from a full-color RGBA source bitmap.
     */

    const uint8_t alphaThreshold = 0x80;
    uint8_t *row, *pixel;
    unsigned x, y;

    // First pass.. are there any transparent pixels?
    mUsingChromaKey = false;
    for (row = rgba, y = SIZE; y; --y, row += stride)
	for (pixel = row, x = SIZE; x; --x, pixel += 4)
	    if (pixel[3] < alphaThreshold)
		mUsingChromaKey = true;
    

    // Second pass.. convert to RGB565, possibly with colorkey.
    RGB565 *dest = mPixels;
    for (row = rgba, y = SIZE; y; --y, row += stride)
	for (pixel = row, x = SIZE; x; --x, pixel += 4) {
	    RGB565 color = RGB565(pixel);

	    if (!mUsingChromaKey) {
		// No transparency in the image, we're allowed to use any color.
		*dest = color;
	    }
	    else if (pixel[3] < alphaThreshold) {
		// Pixel is actually transparent
		*dest = CHROMA_KEY;
	    }
	    else if ((color.value & 0xFF) == (CHROMA_KEY & 0xFF)) {
		/*
		 * Pixel isn't transparent, but it would look like
		 * the chromakey to our firmware's 8-bit comparison.
		 * Modify the color slightly.
		 */
		*dest = color.wiggle();
	    }
	    else {
		// Opaque pixel
		*dest = color;
	    }

	    dest++;
	}	    
}

void Tile::constructPalette(void)
{
    std::set<RGB565> colors;

    for (unsigned i = 0; i < PIXELS; i++)
	colors.insert(mPixels[i]);
    
    mPalette.numColors = colors.size();

    if (mPalette.hasLUT()) {
	unsigned index = 0;
	for (std::set<RGB565>::iterator i = colors.begin(); i != colors.end(); i++)
	    mPalette.colors[index++] = *i;
    }
}

double Tile::errorMetric(const Tile &other) const
{
    /*
     * This is a rather ad-hoc attempt at a perceptually-optimized
     * error metric. It is a form of multi-scale MSE metric, plus we
     * attempt to take into account the geometric structure of a tile
     * by comparing a luminance edge detection map.
     */

    return (0.30 * sobelError(other) +            // Contrast structure
	    0.10 * meanSquaredError(other, 1) +   // Fine color
	    1.00 * meanSquaredError(other, 4));   // Coarse color
}

double Tile::meanSquaredError(const Tile &other, int scale) const
{
    /*
     * This is a mean squared error metric which can operate at
     * different scales. This operates as if the tiles in question
     * had been scaled down by a factor of 'scale' first.
     *
     * This only really makes sense with scales of 1, 2, 4, or 8.
     */

    int scale2 = scale * scale;
    double error = 0;
    unsigned total = 0;

    // Y/X decimation blocks
    for (unsigned y1 = 0; y1 < SIZE; y1 += scale) {
	for (unsigned x1 = 0; x1 < SIZE; x1 += scale) {
	    
	    CIELab acc1, acc2;

	    // Y/X pixels
	    for (unsigned y2 = y1; y2 < y1 + scale; y2++) {
		for (unsigned x2 = x1; x2 < x1 + scale; x2++) {
		    acc1 += pixel(x2, y2);
		    acc2 += other.pixel(x2, y2);
		}
	    }

	    // Average the pixels in each decimation block
	    acc1 /= scale2;
	    acc2 /= scale2;

	    error += acc1.meanSquaredError(acc2);
	    total++;
	}
    }

    return error / total;
}

double Tile::sobelError(const Tile &other) const
{
    /*
     * An error metric based solely on detecting structural luminance
     * differences using the Sobel operator.
     */

    double error = 0;

    for (unsigned y = 0; y < SIZE; y++)
	for (unsigned x = 0; x < SIZE; x++) {
	    double gx = sobelGx(x, y) - other.sobelGx(x, y);
	    double gy = sobelGy(x, y) - other.sobelGy(x, y);
	    error += gx * gx + gy * gy;
	}

    return error / PIXELS;
}

void Tile::render(uint8_t *rgba, size_t stride) const
{
    // Draw this tile to an RGBA framebuffer, for proofing purposes

    unsigned x, y;
    uint8_t *row, *dest;

    for (y = 0, row = rgba; y < SIZE; y++, row += stride)
	for (x = 0, dest = row; x < SIZE; x++, dest += 4) {
	    RGB565 color = pixel(x, y);
	    dest[0] = color.red();
	    dest[1] = color.green();
	    dest[2] = color.blue();
	    dest[3] = 0xFF;
	}
}

TileRef Tile::reduce(ColorReducer &reducer, double maxMSE) const
{
    /*
     * Reduce a tile's color palette, using a completed optimized
     * ColorReducer instance.  Uses maxMSE to provide hysteresis for
     * the color selections, emphasizing color runs when possible.
     */

    TileRef result = TileRef(new Tile(mUsingChromaKey));
    RGB565 run;

    // Hysteresis amount
    maxMSE *= 0.9;

    for (unsigned i = 0; i < PIXELS; i++) {
	RGB565 color = reducer.nearest(mPixels[i]);
	double error = CIELab(color).meanSquaredError(CIELab(run));
	if (error > maxMSE)
	    run = color;
	result->mPixels[i] = run;
    }

    return result;
}

TilePalette::TilePalette()
    : numColors(0)
    {}

void TileStack::add(TileRef t)
{
    tiles.push_back(t);
    cache = TileRef();
}

TileRef TileStack::median()
{
    /*
     * Create a new tile based on the per-pixel median of every tile in the set.
     */

    if (optimized)
	return optimized;
    if (cache)
	return cache;

    Tile *t = new Tile();
    std::vector<RGB565> colors(tiles.size());

    // The median algorithm repeats independently for every pixel in the tile.
    for (unsigned i = 0; i < Tile::PIXELS; i++) {

	// Collect possible colors for this pixel
	for (unsigned j = 0; j < tiles.size(); j++)
	    colors[j] = tiles[j]->pixel(i);

	// Sort along the major axis
	int major = CIELab::findMajorAxis(&colors[0], colors.size());
	std::sort(colors.begin(), colors.end(),
		  CIELab::sortAxis(major));

	// Pick the median color
	t->mPixels[i] = colors[colors.size() >> 1];
    }

    cache = TileRef(t);

    /*
     * Some individual tiles will receive a huge number of references.
     * This is a bit of a hack to prevent a single tile stack from growing
     * boundlessly. If we just calculated a median for a stack over a preset
     * maximum size, we replace the entire stack with copies of the median
     * image, in order to give that median significant (but not absolute)
     * statistical weight.
     *
     * The problem with this is that a tile's median is no longer
     * computed globally across the entire asset group, but is instead
     * more of a sliding window. But for the kinds of heavily repeated
     * tiles that this algorithm will apply to, maybe this isn't an
     * issue?
     */

    if (tiles.size() > MAX_SIZE) {
	tiles = std::vector<TileRef>(MAX_SIZE / 2, cache);
    }

    return cache;
}

TileStackRef TilePool::add(TileRef t)
{
    /*
     * Add a new tile to the pool, and try to optimize it if we can.
     *
     * Returns a TileStack reference which can be used now or
     * later (within the lifetime of the TilePool) to retrieve the
     * latest median for all tiles we've consolidated with the
     * provided one, and to refer to this particular unique set of
     * tiles.
     */

    double mse;
    TileStackRef c = closest(t, mse);

    totalTiles++;

    if (!c || mse > maxMSE) {
	// Too far away. Start a new set.

	c = TileStackRef(new TileStack());
	sets.push_back(c);
    }

    if (!(totalTiles % 256)) {
	fprintf(stderr, "Optimizing tiles... %u sets / %u total (%.03f %%)\n",
		(unsigned) sets.size(), totalTiles, sets.size() * 100.0 / totalTiles);
    }

    c->add(t);
    return c;
}

TileStackRef TilePool::closest(TileRef t, double &mse)
{
    /*
     * Search for the closest tile set for the provided tile image.
     * Returns the tile set itsef, if any was found, and the MSE
     * between the provided tile and that tile's current median.
     */

    double distance = DBL_MAX;
    TileStackRef closest;

    for (Collection::iterator i = sets.begin(); i != sets.end(); i++) {
	double err = (*i)->median()->errorMetric(*t);
	if (err < distance) {
	    distance = err;
	    closest = *i;
	}
    }

    mse = distance;
    return closest;
}

TileGrid::TileGrid(TilePool *pool)
    : mPool(pool), mWidth(0), mHeight(0)
    {}

void TileGrid::load(uint8_t *rgba, size_t stride, unsigned width, unsigned height)
{
    mWidth = width / Tile::SIZE;
    mHeight = height / Tile::SIZE;
    tiles.resize(mWidth * mHeight);

    for (unsigned y = 0; y < mHeight; y++)
	for (unsigned x = 0; x < mWidth; x++) {
	    TileRef t = TileRef(new Tile(rgba + (x * Tile::SIZE * 4) +
					 (y * Tile::SIZE * stride), stride));
	    tiles[x + y * mWidth] = mPool->add(t);
	}
}

bool TileGrid::load(const char *pngFilename)
{
    std::vector<uint8_t> image;
    std::vector<uint8_t> png;
    LodePNG::Decoder decoder;
 
    LodePNG::loadFile(png, pngFilename);
    if (png.empty())
	return false;

    decoder.decode(image, png);
    if (image.empty())
	return false;

    load(&image[0], decoder.getWidth() * 4, decoder.getWidth(), decoder.getHeight());

    return true;
}

void TileGrid::render(uint8_t *rgba, size_t stride)
{
    // Draw this image to an RGBA framebuffer, for proofing purposes
    
    for (unsigned y = 0; y < mHeight; y++)
	for (unsigned x = 0; x < mWidth; x++) {
	    TileRef t = tile(x, y)->median();
	    t->render(rgba + (x * Tile::SIZE * 4) + (y * Tile::SIZE * stride), stride);
	}
}

void TilePool::optimize()
{
    /*
     * Global optimizations to apply after filling a tile pool.
     */

    if (maxMSE > 0)
	optimizePalette();

    optimizeOrder();
}

void TilePool::optimizePalette()
{
    ColorReducer reducer;

    // First, add ALL tile data to the reducer's pool    
    for (Collection::iterator i = sets.begin(); i != sets.end(); i++) {
	TileRef t = (*i)->median();
	for (unsigned j = 0; j < Tile::PIXELS; j++)
	    reducer.add(t->pixel(j));
    }

    // Ask the reducer to do its own (slow!) global optimization
    reducer.reduce(maxMSE);

    // Now reduce each tile, using the agreed-upon color palette
    for (Collection::iterator i = sets.begin(); i != sets.end(); i++) {
	TileStackRef ts = *i;
	ts->optimized = ts->median()->reduce(reducer, maxMSE);
    }
}

void TilePool::render(uint8_t *rgba, size_t stride, unsigned width)
{
    // Draw all tiles in this pool to an RGBA framebuffer, for proofing purposes
    
    unsigned x = 0, y = 0;
    for (Collection::iterator i = sets.begin(); i != sets.end(); i++) {
	TileRef t = (*i)->median();
	t->render(rgba + (x * Tile::SIZE * 4) + (y * Tile::SIZE * stride), stride);

	x++;
	if (x == width) {
	    x = 0;
	    y++;
	}
    }
}

unsigned TilePool::orderingCost()
{
    /*
     * Measure the current cost associated with the ordering of tiles
     * in our pool. This is based on the expected encoding of a tile's
     * colors, given those tiles coming before it in the pool.
     *
     * This assumes that the color LUT is maintained using an LRU
     * eviction policy.
     *
     * XXX: Once we're actually encoding this in a flash loadstream,
     *      share code here so we don't risk the optimizer and the
     *      compressor getting out of sync.
     */

    const unsigned modeSwitchCost = 1;   // Opcode/runlength byte
    const unsigned lutLoadCost = 3;      // Opcode/index byte, plus 16-bit color

    unsigned cost = 0;
    TilePalette::ColorMode mode = TilePalette::CM_INVALID;
    std::list<RGB565> lut;

    for (Collection::iterator i = sets.begin(); i != sets.end(); i++) {
	const TilePalette &pal = (*i)->median()->palette();
	TilePalette::ColorMode nextMode = pal.colorMode();

	if (nextMode != mode) {
	    cost += modeSwitchCost;
	    mode = nextMode;
	}

	// Account for each color in this tile
	for (unsigned c = 0; c < pal.numColors; c++) {
	    RGB565 color = pal.colors[c];
	    std::list<RGB565>::iterator li;

	    for (li = lut.begin(); li != lut.end(); li++)
		if (*li == color)
		    break;

	    if (li == lut.end()) {
		// Color not found, we'll be adding something new.
		cost += lutLoadCost;

		// Do we need to make room for it?
		if (lut.size() == TilePalette::LUT_MAX)
		    lut.pop_back();

	    } else {
		// Found it. Remove so we can bump it.
		lut.erase(li);
	    }

	    // This color is now most recent
	    lut.push_front(c);
	}
    }

    return cost;
}

void TilePool::optimizeOrder()
{
    /*
     * Optimize the order of our tiles within the pool. It turns out
     * that this is a very computationally difficult problem- it's
     * actually an asymmetric travelling salesman problem!
     *
     * This is a silly hill-climbing algorithm based on swapping
     * halves of our pool. It's inspired by 2-opt, but it isn't
     * actually 2-opt because this is an ATSP, and we have no desire
     * to reverse the order with which we're visiting tiles on every
     * iteration.
     *
     * XXX: Silly hill climbing was very silly. Scrapped all that,
     *      let's try a greedy algorithm tomorrow.
     */
}
