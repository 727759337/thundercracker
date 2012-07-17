/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <string.h>
#include <assert.h>

#include <protocol.h>
#include "tilecodec.h"

namespace Stir {

TileCodecLUT::TileCodecLUT()
{
    for (unsigned i = 0; i < LUT_MAX; i++)
        mru[i] = i;
}

unsigned TileCodecLUT::encode(const TilePalette &pal)
{
    uint16_t newColors;
    return encode(pal, newColors);
}

unsigned TileCodecLUT::encode(const TilePalette &pal, uint16_t &newColors)
{
    /*
     * Modify the current LUT state in order to accomodate the given
     * tile palette, and measure the associated cost (in bytes).
     */

    const unsigned runBreakCost = 1;   // Breaking a run of tiles, need a new opcode/runlength byte
    const unsigned lutLoadCost = 3;    // Opcode/index byte, plus 16-bit color

    unsigned cost = 0;
    TilePalette::ColorMode mode = pal.colorMode();
    unsigned maxLUTIndex = pal.maxLUTIndex();

    newColors = 0;

    // Account for each color in this tile
    if (pal.hasLUT()) {
        unsigned numMissing = 0;
        RGB565 missing[LUT_MAX];

        /*
         * Reverse-iterate over the tile's colors, so that the most
         * popular colors (at the beginning of the palette) will end
         * up at the beginning of the MRU list afterwards.
         */

        for (int c = pal.numColors - 1; c >= 0; c--) {
            RGB565 color = pal.colors[c];
            int index = findColor(color, maxLUTIndex);

            if (index < 0) {
                // Don't have this color yet, or it isn't reachable.
                // Add it to a temporary list of colors that we need
                missing[numMissing++] = color;

            } else {
                // We already have this color in the LUT! Bump it to the head of the MRU list.

                for (unsigned i = 0; i < LUT_MAX - 1; i++)
                    if (mru[i] == index) {
                        bumpMRU(i, index);
                        break;
                    }
            }
        }

        /*
         * After bumping any colors that we do need, add colors we
         * don't yet have. We replace starting with the least recently
         * used LUT entries and the least popular missing colors. The
         * most popular missing colors will end up at the front of the
         * MRU list.
         *
         * Because of the reversal above, the most popular missing
         * colors are at the end of the vector. So, we iterate in
         * reverse once more.
         */

        while (numMissing) {

            // Find the oldest index that is reachable by this tile's LUT
            unsigned index;
            for (unsigned i = 0; i < LUT_MAX; i++) {
                index = mru[i];
                if (index <= maxLUTIndex) {
                    bumpMRU(i, index);
                    break;
                }
            }

            colors[index] = missing[--numMissing];
            newColors |= 1 << index;
            cost += lutLoadCost;
        }
    }

    // We have to break a run if we're switching modes OR reloading a LUT entry.
    if (mode != lastMode || cost != 0)
        cost += runBreakCost;
    lastMode = mode;

    return cost;
}

RLECodec4::RLECodec4()
    : runNybble(0), bufferedNybble(0), isNybbleBuffered(false), runCount(0)
    {}
    
void RLECodec4::encode(uint8_t nybble, std::vector<uint8_t>& out)
{
    if (nybble != runNybble || runCount == MAX_RUN)
        encodeRun(out);

    runNybble = nybble;
    runCount++;   
}

void RLECodec4::flush(std::vector<uint8_t>& out)
{
    encodeRun(out, true);
    if (isNybbleBuffered)
        encodeNybble(0, out);
}

void RLECodec4::encodeNybble(uint8_t value, std::vector<uint8_t>& out)
{
    if (isNybbleBuffered) {
        out.push_back(bufferedNybble | (value << 4));
        isNybbleBuffered = false;
    } else {
        bufferedNybble = value;
        isNybbleBuffered = true;
    }
}

void RLECodec4::encodeRun(std::vector<uint8_t>& out, bool terminal)
{
    if (runCount > 0) {
        encodeNybble(runNybble, out);

        if (runCount > 1) {
            encodeNybble(runNybble, out);

            if (runCount == 2 && !terminal)
                // Null runs can be omitted at the end of an encoding block
                encodeNybble(0, out);

            else if (runCount > 2)
                encodeNybble(runCount - 2, out);
        }
    }

    runCount = 0;
}

TileCodec::TileCodec(std::vector<uint8_t>& buffer)
    : out(buffer), opIsBuffered(false), 
      tileCount(0),
      paddedOutputMin(0), currentAddress(0),
      statBucket(TilePalette::CM_INVALID)
{
    memset(&stats, 0, sizeof stats);
}

void TileCodec::encode(const TileRef tile)
{
    currentAddress.linear += FlashAddress::TILE_SIZE;

    /*
     * First off, encode LUT changes.
     */

    const TilePalette &pal = tile->palette();
    uint16_t newColors;
    lut.encode(pal, newColors);
    if (newColors)
        encodeLUT(newColors);

    /*
     * Now encode the tile bitmap data. We do this in a
     * format-specific manner, then try to combine the result with our
     * current opcode if we can.
     */

    TilePalette::ColorMode colorMode = pal.colorMode();
    uint8_t tileOpcode;

    switch (colorMode) {

        /* Trivial one-color tile. Just emit a bare FLS_OP_TILE_P0 opcode. */
    case TilePalette::CM_LUT1:
        encodeOp(FLS_OP_TILE_P0 | lut.findColor(pal.colors[0]));
        newStatsTile(colorMode);
        return;

        /* Repeatable tile types */
    case TilePalette::CM_LUT2:  tileOpcode = FLS_OP_TILE_P1_R4; break;
    case TilePalette::CM_LUT4:  tileOpcode = FLS_OP_TILE_P2_R4; break;
    case TilePalette::CM_LUT16: tileOpcode = FLS_OP_TILE_P4_R4; break;
    default:                    tileOpcode = FLS_OP_TILE_P16;   break;
    }

    /*
     * Emit a new opcode only if we have to break this run. Otherwise,
     * extend our existing tile run.
     */

    if (!opIsBuffered
        || tileOpcode != (opcodeBuf & FLS_OP_MASK)
        || (opcodeBuf & FLS_ARG_MASK) == FLS_ARG_MASK)
        encodeOp(tileOpcode);
    else
        opcodeBuf++;

    /*
     * Format-specific tile encoders
     */

    newStatsTile(colorMode);

    switch (tileOpcode) {
    case FLS_OP_TILE_P1_R4:     encodeTileRLE4(tile, 1);        break;
    case FLS_OP_TILE_P2_R4:     encodeTileRLE4(tile, 2);        break;
    case FLS_OP_TILE_P4_R4:     encodeTileRLE4(tile, 4);        break;
    case FLS_OP_TILE_P16:       encodeTileMasked16(tile);       break;
    }
}

void TileCodec::newStatsTile(unsigned bucket)
{
    // Collect stats per-colormode
    statBucket = bucket;
    tileCount++;
}

void TileCodec::flush()
{
    // Flush any final opcode(s)
    flushOp();

    // Respect any minimum padding requirements from previous opcodes
    while (out.size() < paddedOutputMin)
        out.push_back(FLS_OP_NOP);
}

void TileCodec::flushOp()
{
    if (opIsBuffered) {
        if (statBucket != TilePalette::CM_INVALID) {
            stats[statBucket].opcodes++;
            stats[statBucket].dataBytes += dataBuf.size();
            stats[statBucket].tiles += tileCount;
            tileCount = 0;
        }

        rle.flush(dataBuf);

        out.push_back(opcodeBuf);
        out.insert(out.end(), dataBuf.begin(), dataBuf.end());
        dataBuf.clear();
        opIsBuffered = false;
    }
}

void TileCodec::encodeOp(uint8_t op)
{
    flushOp();
    opIsBuffered = true;
    opcodeBuf = op;
}

void TileCodec::encodeLUT(uint16_t newColors)
{
    if (newColors & (newColors - 1)) {
        /*
         * More than one new color. Emit FLS_OP_LUT16.
         */

        encodeOp(FLS_OP_LUT16);
        encodeWord(newColors);

        for (unsigned index = 0; index < 16; index++)
            if (newColors & (1 << index))
                encodeWord(lut.colors[index].value);

    } else {
        /*
         * Exactly one new color. Use FLS_OP_LUT1.
         */

        for (unsigned index = 0; index < 16; index++)
            if (newColors & (1 << index)) {
                encodeOp(FLS_OP_LUT1 | index);
                encodeWord(lut.colors[index].value);
                break;
            }
    }
}

void TileCodec::encodeWord(uint16_t w)
{
    dataBuf.push_back((uint8_t) w);
    dataBuf.push_back((uint8_t) (w >> 8));
}

void TileCodec::reservePadding(unsigned bytes)
{
    /*
     * Ensure that the output will always have at least 'bytes'
     * bytes of data after this point in the stream, including
     * all buffered opcode data.
     */

    // Calculate current length (worst case)
    unsigned len = out.size();

    if (opIsBuffered) {
        // Opcode size, plus worst-case RLE buffer size
        len += 2;

        // Buffered opcode argument data
        len += dataBuf.size();
    }

    paddedOutputMin = std::max(paddedOutputMin, len + bytes);
}

void TileCodec::encodeTileRLE4(const TileRef tile, unsigned bits)
{
    /*
     * Simple indexed-color tiles, compressed using our 4-bit RLE codec.
     */
    
    uint8_t nybble = 0;
    unsigned pixelIndex = 0;
    unsigned bitIndex = 0;

    reservePadding(FLS_MIN_TILE_R4);

    while (pixelIndex < Tile::PIXELS) {

        // Reserve padding before every 16 pixel block
        if (!(pixelIndex & 15))
            reservePadding(FLS_MIN_TILE_P16);

        uint8_t color = lut.findColor(tile->pixel(pixelIndex));
        assert(color < (1 << bits));
        nybble |= color << bitIndex;

        pixelIndex++;
        bitIndex += bits;

        if (bitIndex == 4) {
            rle.encode(nybble, dataBuf);
            nybble = 0;
            bitIndex = 0;
        }
    }
}

void TileCodec::encodeTileMasked16(const TileRef tile)
{
    /*
     * This is a different form of RLE, which encodes repeats at a
     * pixel granularity, but with interleaved repeat information and
     * pixel data.
     *
     * We emit rows which begin with an 8-bit mask, containing between
     * 0 and 8 pixel values. A '1' bit in the mask corresponds to a
     * new pixel value, and a '0' is copied from the previous
     * value.
     *
     * The "current" color for P16 is stored in entry [15] in the LUT.
     */

    for (unsigned y = 0; y < Tile::SIZE; y++) {
        uint8_t mask = 0;

        // Reserve padding before every 16 pixel block
        if (!(y & 3))
            reservePadding(FLS_MIN_TILE_P16);

        for (unsigned x = 0; x < Tile::SIZE; x++)
            if (tile->pixel(x, y) != lut.colors[15]) {
                mask |= 1 << x;
                lut.colors[15] = tile->pixel(x, y);
            }

        dataBuf.push_back(mask);

        for (unsigned x = 0; x < Tile::SIZE; x++)
            if (mask & (1 << x))
                encodeWord(tile->pixel(x, y).value);
    }
}

void TileCodec::dumpStatistics(Logger &log)
{
    log.infoBegin("Tile encoder statistics");

    for (int m = 0; m < TilePalette::CM_COUNT; m++) {
        unsigned compressedSize = stats[m].dataBytes + stats[m].opcodes;
        unsigned uncompressedSize = stats[m].tiles * (Tile::PIXELS * 2);
        double ratio = uncompressedSize ? 100.0 - compressedSize * 100.0 / uncompressedSize : 0;

        log.infoLine("%10s: % 4u ops, % 4u tiles, % 5u bytes, % 5.01f%% compression",
                     TilePalette::colorModeName((TilePalette::ColorMode) m),
                     stats[m].opcodes,
                     stats[m].tiles,
                     stats[m].dataBytes,
                     ratio);
    }

    log.infoEnd();
}

};  // namespace Stir
