/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker firmware
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef BITS_H
#define BITS_H

#include "macros.h"
#include "machine.h"
#include "crc.h"


/**
 * A vector of bits which uses Intrinsic::CLZ to support fast iteration.
 *
 * Supports vectors with any fixed-size number of bits. For sizes <= 32 bits,
 * this is just as efficient as using a bare uint32_t.
 */

template <unsigned tSize>
class BitVector
{
public:
    uint32_t words[(tSize + 31) / 32];

    /// Mark (set to 1) a single bit
    void mark(unsigned index)
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        ASSERT(index < tSize);
        if (NUM_WORDS > 1) {
            unsigned word = index >> 5;
            unsigned bit = index & 31;
            words[word] |= Intrinsic::LZ(bit);
        } else {
            words[0] |= Intrinsic::LZ(index);
        }
    }

    /// Clear (set to 0) a single bit
    void clear(unsigned index)
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        ASSERT(index < tSize);
        if (NUM_WORDS > 1) {
            unsigned word = index >> 5;
            unsigned bit = index & 31;
            words[word] &= ~Intrinsic::LZ(bit);
        } else {
            words[0] &= ~Intrinsic::LZ(index);
        }
    }

    /// Mark (set to 1) a single bit, using atomic operations
    void atomicMark(unsigned index)
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        ASSERT(index < tSize);
        if (NUM_WORDS > 1) {
            unsigned word = index >> 5;
            unsigned bit = index & 31;
            Atomic::Or(words[word], Intrinsic::LZ(bit));
        } else {
            Atomic::Or(words[0], Intrinsic::LZ(index));
        }
    }

    /// Clear (set to 0) a single bit, using atomic operations
    void atomicClear(unsigned index)
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        ASSERT(index < tSize);
        if (NUM_WORDS > 1) {
            unsigned word = index >> 5;
            unsigned bit = index & 31;
            Atomic::And(words[word], ~Intrinsic::LZ(bit));
        } else {
            Atomic::And(words[0], ~Intrinsic::LZ(index));
        }
    }

    /// Mark (set to 1) all bits in the vector
    void mark()
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;
        const unsigned NUM_FULL_WORDS = tSize / 32;
        const unsigned REMAINDER_BITS = tSize & 31;

        STATIC_ASSERT(NUM_FULL_WORDS + 1 == NUM_WORDS ||
                      NUM_FULL_WORDS == NUM_WORDS);

        #ifdef __clang__
        #   pragma clang diagnostic push
        #   pragma clang diagnostic ignored "-Wtautological-compare"
        #   pragma clang diagnostic ignored "-Warray-bounds"
        #endif

        // Set fully-utilized words only
        for (unsigned i = 0; i < NUM_FULL_WORDS; ++i)
            words[i] = -1;

        if (NUM_FULL_WORDS != NUM_WORDS) {
            // Set only bits < tSize in the last word.
            uint32_t mask = ((uint32_t)-1) << ((32 - REMAINDER_BITS) & 31);
            words[NUM_FULL_WORDS] = mask;
        }

        #ifdef __clang__
        #   pragma clang diagnostic pop
        #endif
    }

    /// Invert all bits in the vector
    void invert()
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;
        const unsigned NUM_FULL_WORDS = tSize / 32;
        const unsigned REMAINDER_BITS = tSize & 31;

        STATIC_ASSERT(NUM_FULL_WORDS + 1 == NUM_WORDS ||
                      NUM_FULL_WORDS == NUM_WORDS);

        #ifdef __clang__
        #   pragma clang diagnostic push
        #   pragma clang diagnostic ignored "-Wtautological-compare"
        #endif

        // Set fully-utilized words only
        for (unsigned i = 0; i < NUM_FULL_WORDS; ++i)
            words[i] ^= -1;

        #ifdef __clang__
        #   pragma clang diagnostic pop
        #endif

        if (NUM_FULL_WORDS != NUM_WORDS) {
            // Set only bits < tSize in the last word.
            uint32_t mask = ((uint32_t)-1) << ((32 - REMAINDER_BITS) & 31);
            words[NUM_FULL_WORDS] ^= mask;
        }
    }

    /// Clear (set to 0) all bits in the vector
    void clear()
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        for (unsigned i = 0; i < NUM_WORDS; ++i)
            words[i] = 0;
    }

    /// Is a particular bit marked?
    bool test(unsigned index) const
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        ASSERT(index < tSize);
        if (NUM_WORDS > 1) {
            unsigned word = index >> 5;
            unsigned bit = index & 31;
            return (words[word] & Intrinsic::LZ(bit)) != 0;
        } else {
            return (words[0] & Intrinsic::LZ(index)) != 0;
        }
    }

    /// Is every bit in this vector set to zero?
    bool empty() const
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        if (NUM_WORDS > 1) {
            for (unsigned w = 0; w < NUM_WORDS; w++)
                if (words[w])
                    return false;
            return true;
        } else {
            return words[0] == 0;
        }
    }

    /**
     * Find the lowest index where there's a marked (1) bit.
     * If any marked bits exist, returns true and puts the bit's index
     * in "index". Iff the entire vector is zero, returns false.
     */
    bool findFirst(unsigned &index) const
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        if (NUM_WORDS > 1) {
            for (unsigned w = 0; w < NUM_WORDS; w++) {
                uint32_t v = words[w];
                if (v) {
                    index = (w << 5) | Intrinsic::CLZ(v);
                    ASSERT(index < tSize);
                    return true;
                }
            }
        } else {
            uint32_t v = words[0];
            if (v) {
                index = Intrinsic::CLZ(v);
                ASSERT(index < tSize);
                return true;
            }
        }
        return false;
    }

    /**
     * Find the lowest marked bit. If we find any marked bits,
     * returns true, sets "index" to that bit's index, and clears the
     * bit. This can be used as part of an iteration pattern:
     *
     *       unsigned index;
     *       while (vec.clearFirst(index)) {
     *           doStuff(index);
     *       }
     *
     * This is functionally equivalent to findFirst() followed by
     * clear(), but it's a tiny bit more efficient.
     */
    bool clearFirst(unsigned &index)
    {
        const unsigned NUM_WORDS = (tSize + 31) / 32;

        if (NUM_WORDS > 1) {
            for (unsigned w = 0; w < NUM_WORDS; w++) {
                uint32_t v = words[w];
                if (v) {
                    unsigned bit = Intrinsic::CLZ(v);
                    words[w] ^= Intrinsic::LZ(bit);
                    index = (w << 5) | bit;
                    ASSERT(index < tSize);
                    return true;
                }
            }
        } else {
            uint32_t v = words[0];
            if (v) {
                unsigned bit = Intrinsic::CLZ(v);
                words[0] ^= Intrinsic::LZ(bit);
                index = bit;
                ASSERT(index < tSize);
                return true;
            }
        }
        return false;
    }

    /**
     * Population count.
     * Return the total number of set bits in the vector.
     */
    unsigned popcount() const {

        const unsigned NUM_WORDS = (tSize + 31) / 32;
        unsigned count = 0;

        if (NUM_WORDS > 1) {
            for (unsigned w = 0; w < NUM_WORDS; w++) {
                count += Intrinsic::POPCOUNT(words[w]);
            }
        } else {
            count = Intrinsic::POPCOUNT(words[0]);
        }
        return count;
    }
};


/**
 * 8-bit right rotate
 */
uint8_t ALWAYS_INLINE ROR8(uint8_t a, unsigned shift)
{
    return (a << (8 - shift)) | (a >> shift);
}

static uint8_t computeCheckByte(uint8_t a, uint8_t b);

#endif
