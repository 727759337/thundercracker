#include <stdlib.h>
#include "flashlayer.h"
#include <sifteo/macros.h>

// statics
FlashLayer::CachedBlock FlashLayer::blocks[NUM_BLOCKS];
struct FlashLayer::Stats FlashLayer::stats;

void *FlashLayer::getRegionFromOffset(int offset, int len, int *size)
{
    CachedBlock *b = getCachedBlock(offset);
    if (b == 0) {
        stats.misses++;
        b = getFreeBlock();
        if (b == 0) {
            LOG(("Flashlayer: No free blocks in cache\n"));
            return 0;
        }
        
        int bpos = offset / BLOCK_SIZE;
        ASSERT(fseek(mFile, bpos * BLOCK_SIZE, SEEK_SET) == 0);
        
        size_t result = fread (b->data, 1, BLOCK_SIZE, mFile);
        if (result != (size_t)BLOCK_SIZE) {
            // TODO: Should we pad asset files to 512?  If not, zero out the remainder of this block.
            *size = result;
        }
        
        b->address = bpos * BLOCK_SIZE;
        b->inUse = true;
        b->valid = true;
    } else {
        stats.hits++;
    }

    if (stats.hits + stats.misses >= 1000) {
        DEBUG_LOG(("Flashlayer, stats for last 1000 accesses: %u hits, %u misses\n", stats.hits, stats.misses));
        stats.hits = stats.misses = 0;
    }
    
    int boff = offset % BLOCK_SIZE;
    *size = len;
    
    if ((unsigned)offset + len > b->address + sizeof(b->data)) {
        // requested region crosses block boundary :(
        int valid = b->address + BLOCK_SIZE - offset;
        *size = valid;
    }
    
    return b->data + boff;
}

// File hacks

FILE * FlashLayer::mFile = NULL;

void FlashLayer::init()
{
    mFile = fopen("asegment.bin", "rb");
    ASSERT(mFile != NULL && "ERROR: FlashLayer failed to open asset segment - make sure asegment.bin is next to your executable.\n");
    stats.hits = stats.misses = 0;
}
