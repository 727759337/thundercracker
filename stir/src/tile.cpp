/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <float.h>
#include <algorithm>
#include <set>
#include <map>

#include "tile.h"
#include "tilecodec.h"


/*
 * Tile hash -- A 32-bit FNV-1 hash computed over all pixels.
 * See: http://isthe.com/chongo/tech/comp/fnv/
 */
namespace std {
    namespace tr1 {
        template<> struct hash<Stir::Tile::Identity> {
            std::size_t operator()(Stir::Tile::Identity const &key) const {         
                uint32_t h = 2166136261UL;
                unsigned len = sizeof(key.pixels) / sizeof(uint32_t);
                uint32_t *data = (uint32_t*) &key.pixels[0];
                
                do {
                    h ^= *(data++);
                    h *= 16777619UL;
                } while (--len);
                
                return h;
            }
        };
    }
}

namespace Stir {

std::tr1::unordered_map<Tile::Identity, TileRef> Tile::instances;

Tile::Tile(const Identity &id)
    : mHasSobel(false), mHasDec4(false), mID(id)
    {}

TileRef Tile::instance(const Identity &id)
{
    /*
     * Return an existing Tile matching the given identity, or create a new one if necessary.
     */
     
    std::tr1::unordered_map<Identity, TileRef>::iterator i = instances.find(id);
        
    if (i == instances.end()) {
        TileRef tr(new Tile(id));
        instances[id] = tr;
        return tr;
    } else {
        return i->second;
    }
}

TileRef Tile::instance(const TileOptions &opt, uint8_t *rgba, size_t stride)
{
    /*
     * Load a tile image from a full-color RGBA source bitmap.
     */

    Identity id;
    const uint8_t alphaThreshold = 0x80;
    uint8_t *row, *pixel;
    unsigned x, y;
    
    id.options = opt;

    RGB565 *dest = id.pixels;
    for (row = rgba, y = SIZE; y; --y, row += stride)
        for (pixel = row, x = SIZE; x; --x, pixel += 4) {
            RGB565 color = RGB565(pixel);

            if (!id.options.chromaKey) {
                // No transparency in the image, we're allowed to use any color.
                *dest = color;
            }
            else if (pixel[3] < alphaThreshold) {
                /*
                 * Pixel is actually transparent.
                 *
                 * Use CHROMA_KEY as the MSB, and encode some information
                 * in the LSB. Currently, the firmware wants to know if the
                 * rest of this scanline on the current tile is entirely
                 * transparent. If so, we set the End Of Line bit.
                 */

                dest->value = CHROMA_KEY << 8;
                
                bool eol = true;
                for (unsigned xr = 0; xr < x; xr++) {
                    if (pixel[3 + xr*4] >= alphaThreshold) {
                        eol = false;
                        break;
                    }
                }
                
                if (eol)
                    dest->value |= CKEY_BIT_EOL;
            }
            else if ((color.value >> 8) == CHROMA_KEY) {
                /*
                 * Pixel isn't transparent, but it would look like
                 * the chromakey to our firmware's 8-bit comparison.
                 * Modify the color slightly (toggle the red LSB).
                 */

                *dest = (uint16_t)(color.value ^ (1 << 11));
            }
            else {
                // Opaque pixel
                *dest = color;
            }

            dest++;
        }    

    return instance(id);
}

double TileOptions::getMaxMSE() const
{
    /*
     * Convert from the user-visible "quality" to a maximum MSE limit.
     * User-visible quality numbers are in the range [0, 10]. Larger
     * is better. I've tried to scale this so that it's intuitively
     * similar to the quality metrics used by e.g. JPEG.
     */

    double err = 10.0 - std::min(10.0, std::max(0.0, quality));
    return err * 500;
}

void Tile::constructPalette(void)
{
    /*
     * Create a TilePalette for this particular tile. If the tile has few enough colors
     * that we can make a LUT, the palette will include a populated LUT. Additionally,
     * as a heuristic to help us provide color matches across transitions from higher
     * tile color depth to lower color depth, we try to order the most popular colors
     * at the begining of the LUT.
     */

    // Tracks unique colors, and their frequencies
    std::map<RGB565, unsigned> colors;

    for (unsigned i = 0; i < PIXELS; i++)
        colors[mID.pixels[i]] = colors[mID.pixels[i]] + 1;

    mPalette.numColors = colors.size();

    if (mPalette.hasLUT()) {
        // Sort the colors using an inverse mapping.

        std::multimap<unsigned, RGB565> lutSorter;
        for (std::map<RGB565, unsigned>::iterator i = colors.begin(); i != colors.end(); i++)
            lutSorter.insert(std::pair<unsigned, RGB565>(i->second, i->first));

        unsigned index = 0;
        for (std::multimap<unsigned, RGB565>::reverse_iterator i = lutSorter.rbegin();
             i != lutSorter.rend(); i++)
            mPalette.colors[index++] = i->second;
    }
}

void Tile::constructSobel()
{
    /*
     * Build a Sobel gradient approximation matrix for this tile.
     *
     * See: http://en.wikipedia.org/wiki/Sobel_operator
     */

    mHasSobel = true;
    mSobelTotal = 0;

    unsigned i = 0;
    for (unsigned y = 0; y < SIZE; y++)
        for (unsigned x = 0; x < SIZE; x++, i++) {

            // Luminance of eight neighbor pixels
            float l00 = CIELab(pixelWrap(x-1, y-1)).L;
            float l10 = CIELab(pixelWrap(x  , y-1)).L;
            float l20 = CIELab(pixelWrap(x+1, y-1)).L;
            float l01 = CIELab(pixelWrap(x-1, y  )).L;
            float l21 = CIELab(pixelWrap(x+1, y  )).L;
            float l02 = CIELab(pixelWrap(x-1, y+1)).L;
            float l12 = CIELab(pixelWrap(x  , y+1)).L;
            float l22 = CIELab(pixelWrap(x+1, y+1)).L;

            mSobelGx[i] = -l00 +l20 -l01 -l01 +l21 +l21 -l02 +l22;
            mSobelGy[i] = -l00 +l02 -l10 -l10 +l12 +l12 -l20 +l22;

            mSobelTotal += mSobelGx[i] * mSobelGx[i];
            mSobelTotal += mSobelGy[i] * mSobelGy[i];
        }

#ifdef DEBUG_SOBEL
    for (i = 0; i < PIXELS; i++) {
        int x = std::max(0, std::min(255, (int)(128 + mSobelGx[i])));
        int y = std::max(0, std::min(255, (int)(128 + mSobelGy[i])));
        mPixels[i] = RGB565(x, y, (x+y)/2);
    }
#endif
}    

void Tile::constructDec4()
{
    /*
     * Build a decimated 2x2 pixel representation of this tile, using
     * averaged CIELab colors.
     */

    const unsigned scale = SIZE / 2;
    unsigned i = 0;

    mHasDec4 = true;

    for (unsigned y1 = 0; y1 < SIZE; y1 += scale)
        for (unsigned x1 = 0; x1 < SIZE; x1 += scale) {
            CIELab acc;

            // Y/X pixels
            for (unsigned y2 = y1; y2 < y1 + scale; y2++)
                for (unsigned x2 = x1; x2 < x1 + scale; x2++)
                    acc += pixel(x2, y2);
            
            acc /= scale * scale;
            mDec4[i++] = acc;
        }

#ifdef DEBUG_DEC4
    for (unsigned y = 0; y < SIZE; y++)
        for (unsigned x = 0; x < SIZE; x++)
            mPixels[x + (y * SIZE)] = mDec4[x/scale + y/scale * 2].rgb();
#endif
}

double Tile::errorMetric(Tile &other, double limit)
{
    /*
     * This is a rather ad-hoc attempt at a perceptually-optimized
     * error metric. It is a form of multi-scale MSE metric, plus we
     * attempt to take into account the geometric structure of a tile
     * by comparing a luminance edge detection map.
     *
     * If we exceed 'limit', the test can exit early. This lets us
     * calculate the easy metrics first, and skip the rest if we're
     * already over.
     */

    double error = 0;

    error += 0.450 * coarseMSE(other);
    if (error > limit)
        return DBL_MAX;

    error += 0.025 * fineMSE(other);
    if (error > limit)
        return DBL_MAX;

    error += 5.00 * sobelError(other);

    return error * 60.0;
}

double Tile::fineMSE(Tile &other)
{
    /*
     * A normal pixel-wise mean squared error metric.
     */

    double error = 0;

    for (unsigned i = 0; i < PIXELS; i++)
        error += CIELab(mID.pixels[i]).meanSquaredError(CIELab(other.mID.pixels[i]));

    return error / PIXELS;
}

double Tile::coarseMSE(Tile &other)
{
    /*
     * A reduced scale MSE metric using the 2x2 pixel decimated version of our tile.
     */

    double error = 0;

    if (!mHasDec4)
        constructDec4();
    if (!other.mHasDec4)
        other.constructDec4();

    for (unsigned i = 0; i < 4; i++)
        error += mDec4[i].meanSquaredError(other.mDec4[i]);

    return error / 4;
}

double Tile::sobelError(Tile &other)
{
    /*
     * An error metric based solely on detecting structural luminance
     * differences using the Sobel operator.
     */

    double error = 0;

    if (!mHasSobel)
        constructSobel();
    if (!other.mHasSobel)
        other.constructSobel();

    for (unsigned i = 0; i < PIXELS; i++) {
        double gx = mSobelGx[i] - other.mSobelGx[i];
        double gy = mSobelGy[i] - other.mSobelGy[i];

        error += gx * gx + gy * gy;
    }
    
    // Contrast difference over total contrast
    return error / (1 + mSobelTotal + other.mSobelTotal);
}

TileRef Tile::reduce(ColorReducer &reducer) const
{
    /*
     * Reduce a tile's color palette, using a completed optimized
     * ColorReducer instance.  Uses maxMSE to provide hysteresis for
     * the color selections, emphasizing color runs when possible.
     */

    RGB565 run;

    Identity reduced;   
    reduced.options = mID.options;

    // Hysteresis amount
    double limit = mID.options.getMaxMSE() * 0.05;
    
    for (unsigned i = 0; i < PIXELS; i++) {
        RGB565 original = mID.pixels[i];
        
        if ((original.value >> 8) == CHROMA_KEY) {
            // Don't touch it if this is a special keyed color
            reduced.pixels[i] = original;
        } else {
            RGB565 color = reducer.nearest(original);
            double error = CIELab(color).meanSquaredError(CIELab(run));
            if (error > limit)
                run = color;
            reduced.pixels[i] = run;
        }
    }

    return instance(reduced);
}

TilePalette::TilePalette()
    : numColors(0)
    {}

const char *TilePalette::colorModeName(ColorMode m)
{
    switch (m) {
    case CM_LUT1:       return "CM_LUT1";
    case CM_LUT2:       return "CM_LUT2";
    case CM_LUT4:       return "CM_LUT4";
    case CM_LUT16:      return "CM_LUT16";
    case CM_TRUE:       return "CM_TRUE";
    default:            return "<invalid>";
    }
}

TileStack::TileStack()
    : index(NO_INDEX), mPinned(false)
    {}

void TileStack::add(TileRef t)
{
    const double epsilon = 1e-3;
    double maxMSE = t->options().getMaxMSE();

    if (maxMSE < epsilon) {
        // Lossless. Replace the entire stack, yielding a trivial median.
        tiles.clear();
    }

    // Add to stack, invalidating cache
    tiles.push_back(t);
    cache = TileRef();

    // A stack with any pinned tiles in it is itself pinned.
    if (t->options().pinned)
        mPinned = true;
}

TileRef TileStack::median()
{
    /*
     * Create a new tile based on the per-pixel median of every tile in the set.
     */

    if (cache)
        return cache;
        
    if (tiles.size() == 1) {
        // Special-case for a single-tile stack. No copy, just add a reference
        cache = TileRef(tiles[0]);

    } else {
        // General-case median algorithm

        Tile::Identity median;
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
            median.pixels[i] = colors[colors.size() >> 1];
        }

        cache = Tile::instance(median);

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
    }

    return cache;
}

TileStack* TilePool::closest(TileRef t)
{
    /*
     * Search for the closest tile set for the provided tile image.
     * Returns the tile stack, if any was found which meets the tile's
     * stated maximum MSE requirement.
     */

    const double epsilon = 1e-3;
    double distance = t->options().getMaxMSE();
    TileStack *closest = NULL;

    for (std::list<TileStack>::iterator i = stackList.begin(); i != stackList.end(); i++) {
        double err = i->median()->errorMetric(*t, distance);

        if (err <= distance) {
            distance = err;
            closest = &*i;

            if (distance < epsilon) {
                // Not going to improve on this; early out.
                break;
            }
        }
    }

    return closest;
}

TileGrid::TileGrid(TilePool *pool)
    : mPool(pool), mWidth(0), mHeight(0)
    {}

void TileGrid::load(const TileOptions &opt, uint8_t *rgba,
                    size_t stride, unsigned width, unsigned height)
{
    mWidth = width / Tile::SIZE;
    mHeight = height / Tile::SIZE;
    tiles.resize(mWidth * mHeight);

    for (unsigned y = 0; y < mHeight; y++)
        for (unsigned x = 0; x < mWidth; x++) {
            TileRef t = Tile::instance(opt,
                                       rgba + (x * Tile::SIZE * 4) +
                                       (y * Tile::SIZE * stride),
                                       stride);
            tiles[x + y * mWidth] = mPool->add(t);
        }
}

void TilePool::optimize(Logger &log)
{
    /*
     * Global optimizations to apply after filling a tile pool.
     */

    optimizePalette(log);
    optimizeTiles(log);
    optimizeOrder(log);
}

void TilePool::optimizePalette(Logger &log)
{
    /*
     * Palette optimization, for use after add() but before optimizeTiles().
     *
     * This operates on all tile images in 'tiles', replacing each
     * with a reduced-color version created using a global color
     * palette optimization process.
     */

    ColorReducer reducer;
    bool needReduction = false;
    const double epsilon = 1e-3;

    // First, add ALL tile data to the reducer's pool    
    for (std::vector<TileRef>::iterator i = tiles.begin(); i != tiles.end(); i++) {
        double maxMSE = (*i)->options().getMaxMSE();
        
        if (maxMSE > epsilon) {
            // Lossy mode
            
            needReduction = true;
            for (unsigned j = 0; j < Tile::PIXELS; j++)
                reducer.add((*i)->pixel(j), maxMSE);
        }
    }
    
    // Skip the rest if all tiles are lossless
    if (needReduction) {
 
        // Ask the reducer to do its own (slow!) global optimization
        reducer.reduce(log);

        // Now reduce each tile, using the agreed-upon color palette
        for (std::vector<TileRef>::iterator i = tiles.begin(); i != tiles.end(); i++) {
            double maxMSE = (*i)->options().getMaxMSE();        
            if (maxMSE > epsilon)
                *i = (*i)->reduce(reducer);
        }
    }
}

void TilePool::optimizeTiles(Logger &log)
{
    /*
     * The tile optimizer is a greedy algorithm that works in two
     * passes: First, we collect tiles into stacks, using an MSE
     * comparison based on the stack's *current* median at the
     * time. This produces a good estimate of the stacks we'll need,
     * but there is no guarantee that the stack we place a tile in
     * will be the most optimal stack for that tile, or that each
     * stack median turns out to be unique.
     *
     * After this collection pass, we run a second pass which does not
     * modify the stored medians, but just chooses the best stack for
     * each input tile. In this stage, it is possible that not every
     * input stack will be used.
     *
     * Collect individual tiles, from "tiles", into TileStacks, stored
     * in 'stackList' and 'stackIndex'.
     */

    std::tr1::unordered_set<TileStack *> activeStacks;

    stackList.clear();
    stackIndex.clear();
    stackIndex.resize(tiles.size());

    log.taskBegin("Gathering pinned tiles");
    optimizeTilesPass(log, activeStacks, true, true);
    log.taskEnd();

    log.taskBegin("Gathering unpinned tiles");
    optimizeTilesPass(log, activeStacks, true, false);
    log.taskEnd();

    log.taskBegin("Optimizing tiles");
    optimizeTilesPass(log, activeStacks, false, false);
    log.taskEnd();
}
    
void TilePool::optimizeTilesPass(Logger &log,
                                 std::tr1::unordered_set<TileStack *> &activeStacks,
                                 bool gather, bool pinned)
{
    // A single pass from the multi-pass optimizeTiles() algorithm

    std::tr1::unordered_map<Tile *, TileStack *> memo;
    
    for (Serial serial = 0; serial < tiles.size(); serial++) {
        TileRef tr = tiles[serial];

        if (tr->options().pinned == pinned) {
            TileStack *c = NULL;

            if (!pinned) {
                /*
                 * Assuming we aren't gathering pinned tiles, we start by looking for
                 * the closest existing stack. This is a very slow step, but we can
                 * save a lot of time on common datasets (with plenty of duplicated tiles)
                 * by memoizing the results.
                 *
                 * This memoization is easy because tiles are unique and immutable.
                 * We only keep the memo for the duration of one optimization pass; the whole
                 * point of a multi-pass algorithm is that we expect the result of closest()
                 * to change between the gathering and optimization passes.
                 *
                 * Note that this memoization does actually change the algorithmic complexity
                 * of our optimization passes. Instead of doing what may be an O(N^2) algorithm
                 * on each pass, this is significantly faster (up to O(N) in the best case). We
                 * end up deferring much more of the optimization to the second pass, whereas
                 * without memoization we end up getting very close to a solution in one pass,
                 * at the expense of quite a lot of performance.
                 *
                 * So in our case, two passes can be much faster than one. Yay.
                 */

                std::tr1::unordered_map<Tile *, TileStack *>::iterator i = memo.find(&*tr);
                if (i == memo.end()) {
                    c = closest(tr);
                    memo[&*tr] = c;
                } else {
                    c = memo[&*tr];
                }
            }           

            if (!c) {
                // Need to create a fresh stack
                stackList.push_back(TileStack());
                c = &stackList.back();
                c->add(tr);
            } else if (gather) {
                // Add to an existing stack
                c->add(tr);
            }

            if (!gather || pinned) {
                // Remember this stack, we've selected it for good.
                
                assert(c != NULL);
                stackIndex[serial] = c; 
                activeStacks.insert(c);
            }
        }

        if (serial == tiles.size() - 1 || !(serial % 128)) {
            unsigned stacks = gather ? stackList.size() : activeStacks.size();
            log.taskProgress("%u stacks (%.03f%% of total)", stacks,
                             stacks * 100.0 / tiles.size());
        }
    }

    if (!gather) {
        // Permanently delete unused stacks

        std::list<TileStack>::iterator i = stackList.begin();

        while (i != stackList.end()) {
            std::list<TileStack>::iterator j = i;
            i++;

            if (!activeStacks.count(&*j))
                stackList.erase(j);
        }
    }
}

void TilePool::optimizeOrder(Logger &log)
{
    /*
     * Optimize the order of our tiles within the pool. It turns out
     * that this is a very computationally difficult problem- it's
     * actually an asymmetric travelling salesman problem!
     *
     * It isn't even remotely computationally feasible to come up with
     * a globally optimal solution, so we just use a greedy heuristic
     * which tries to pick the best next tile. This uses
     * TileCodecLUT::encode() as a cost metric.
     *
     * This is where we assign an index to each stack, and build the
     * stackArray.
     */

    std::list<TileStack> newOrder;
    TileCodecLUT codec;
    unsigned totalCost = 0;
    bool pinned = true;

    log.taskBegin("Optimizing tile order");

    stackArray.clear();

    while (!stackList.empty()) {
        std::list<TileStack>::iterator chosen;

        if (pinned && stackList.front().isPinned()) {
            /*
             * We found a consecutive pair of pinned tiles. We're obligated to maintain
             * the ordering of these tiles, so there's no opportunity for optimization.
             */
            
            chosen = stackList.begin();

        } else {
            /*
             * Pick the lowest-cost tile next. Use a forked copy of
             * the codec to ask what the cost would be for each
             * possible choice.
             */

            unsigned bestCost = (unsigned) -1;

            for (std::list<TileStack>::iterator i = stackList.begin(); i != stackList.end(); i++) {
                TileCodecLUT codecFork = codec;
                unsigned cost = codecFork.encode(i->median()->palette());
                if (cost < bestCost) {
                    bestCost = cost;
                    chosen = i;
                }
            }
        }

        // Apply the new codec state permanently
        totalCost += codec.encode(chosen->median()->palette());

        // Pick the best tile, and assign it a permanent index
        pinned = chosen->isPinned();
        chosen->index = stackArray.size();
        stackArray.push_back(&*chosen);
        newOrder.splice(newOrder.end(), stackList, chosen);

        if (!(stackList.size() % 128))
            log.taskProgress("%d tiles (cost %d)",
                             (int) newOrder.size(), totalCost);
    }

    stackList.swap(newOrder);

    log.taskEnd();
}

void TilePool::encode(std::vector<uint8_t>& out, Logger *log)
{
    TileCodec codec(out);

    if (log) {
        log->taskBegin("Encoding tiles");
        log->taskProgress("%d tiles (%d bytes in flash)",
            stackList.size(), stackList.size() * FlashAddress::TILE_SIZE);
    }

    for (std::list<TileStack>::iterator i = stackList.begin(); i != stackList.end(); i++)
        codec.encode(i->median());
    codec.flush();

    if (log) {
        log->taskEnd();
        codec.dumpStatistics(*log);
    }
}


};  // namespace Stir
