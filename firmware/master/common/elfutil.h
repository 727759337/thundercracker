/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef ELF_UTIL_H
#define ELF_UTIL_H

#include <stdint.h>
#include <inttypes.h>

#include "flash_blockcache.h"

namespace Elf {

    struct Segment {
        uint32_t vaddr;
        uint32_t size;
        FlashRange data;
    };

    struct Metadata {
        FlashRange data;
        
        const char *getString(FlashBlockRef &ref, uint16_t key) const;
        const void *get(FlashBlockRef &ref, uint16_t key, uint32_t size) const;
        const void *get(FlashBlockRef &ref, uint16_t key,
            uint32_t minSize, uint32_t &actualSize) const;

        template <typename T>
        inline const T* getValue(FlashBlockRef &ref, uint16_t key) const {
            return reinterpret_cast<const T*>(get(ref, key, sizeof(T)));
        }
    };

    struct ProgramInfo {
        uint32_t entry;
        Segment rodata;
        Segment rwdata;
        Segment bss;
        Metadata meta;

        bool init(const FlashRange &elf);
    };

};

#endif // ELF_UTIL_H
