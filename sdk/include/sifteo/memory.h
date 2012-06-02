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
 * @defgroup memory Memory
 *
 * @{
 */

/// memset(), with an explicit 8-bit data width
inline void memset8(uint8_t *dest, uint8_t value, unsigned count) {
    _SYS_memset8(dest, value, count);
}

/// memset(), with an explicit 16-bit data width
inline void memset16(uint16_t *dest, uint16_t value, unsigned count) {
    _SYS_memset16(dest, value, count);
}

/// memset(), with an explicit 32-bit data width
inline void memset32(uint32_t *dest, uint32_t value, unsigned count) {
    _SYS_memset32(dest, value, count);
}

/// memcpy(), with an explicit 8-bit data width
inline void memcpy8(uint8_t *dest, const uint8_t *src, unsigned count) {
    _SYS_memcpy8(dest, src, count);
}

/// memcpy(), with an explicit 16-bit data width
inline void memcpy16(uint16_t *dest, const uint16_t *src, unsigned count) {
    _SYS_memcpy16(dest, src, count);
}

/// memcpy(), with an explicit 32-bit data width
inline void memcpy32(uint32_t *dest, const uint32_t *src, unsigned count) {
    _SYS_memcpy32(dest, src, count);
}

/// memcmp(), with an explicit 8-bit data width
inline int memcmp8(const uint8_t *a, const uint8_t *b, unsigned count) {
    return _SYS_memcmp8(a, b, count);
}

/// memset(), with an implicit 8-bit data width
inline void memset(uint8_t *dest, uint8_t value, unsigned count) {
    _SYS_memset8(dest, value, count);
}

/// memset(), with an implicit 16-bit data width
inline void memset(uint16_t *dest, uint16_t value, unsigned count) {
    _SYS_memset16(dest, value, count);
}

/// memset(), with an implicit 32-bit data width
inline void memset(uint32_t *dest, uint32_t value, unsigned count) {
    _SYS_memset32(dest, value, count);
}

/// memcpy(), with an implicit 8-bit data width
inline void memcpy(uint8_t *dest, const uint8_t *src, unsigned count) {
    _SYS_memcpy8(dest, src, count);
}

/// memcpy(), with an implicit 16-bit data width
inline void memcpy(uint16_t *dest, const uint16_t *src, unsigned count) {
    _SYS_memcpy16(dest, src, count);
}

/// memcpy(), with an implicit 32-bit data width
inline void memcpy(uint32_t *dest, const uint32_t *src, unsigned count) {
    _SYS_memcpy32(dest, src, count);
}

/// memcmp(), with an implicit 8-bit data width
inline int memcmp(const uint8_t *a, const uint8_t *b, unsigned count) {
    return _SYS_memcmp8(a, b, count);
}

/// Write 'n' zero bytes to memory
inline void bzero(void *s, unsigned count) {
    _SYS_memset8((uint8_t*)s, 0, count);
}

/// One-argument form of bzero: Templatized to zero any fixed-size object
template <typename T>
inline void bzero(T &s) {
    _SYS_memset8((uint8_t*)&s, 0, sizeof s);
}

/**
 * @brief Calculate a 32-bit CRC over a block of memory
 *
 * This is a hardware-accelerated implementation of a variant of the CRC-32
 * Cyclic Redundancy Check algorithm. You can use this to check data integrity,
 * or as a simple hash function for applications that do __not__ require
 * a cryptographic hash.
 *
 * The output of this function is equivalent to the following (slow) reference
 * implementation:
 *
 *     uint32_t slowCrc32(const uint32_t *data, uint32_t count)
 *     {
 *         int32_t crc = -1;
 *         while (count--) {
 *             int32_t word = *(data++);
 *             for (unsigned bit = 32; bit; --bit) {
 *                 crc = (crc << 1) ^ ((crc ^ word) < 0 ? 0x04c11db7 : 0);
 *                 word = word << 1;
 *             }
 *         }
 *         return crc;
 *     }
 *
 * Always operates on an integer number of 32-bit words.
 */
inline uint32_t crc32(const uint32_t *words, unsigned count) {
    return _SYS_crc32(words, count);
}


/**
 * @} end defgroup memory
 */

};  // namespace Sifteo
