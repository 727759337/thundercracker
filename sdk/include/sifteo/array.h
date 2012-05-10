/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the public interface to the Sifteo SDK.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#pragma once
#ifdef NOT_USERSPACE
#   error This is a userspace-only header, not allowed by the current build.
#endif

#include <sifteo/memory.h>
#include <sifteo/macros.h>

namespace Sifteo {


/**
 * A statically sized array.
 *
 * Method naming and conventions are STL-inspired, but designed for very
 * minimal runtime memory footprint.
 */
template <typename T, unsigned _capacity, typename sizeT = uint32_t>
class Array {
public:

    typedef T* iterator;
    typedef const T* const_iterator;
    const unsigned NOT_FOUND = unsigned(-1);

    Array() {
        clear();
    }

    static unsigned capacity() {
        return _capacity;
    }

    unsigned count() const {
        return numItems;
    }

    bool empty() const {
        return count() == 0;
    }

    T operator[](unsigned index) const {
        ASSERT(index < count());
        return items[index];
    }

    T operator[](int index) const {
        ASSERT(0 <= index && index < (int)count());
        return items[index];
    }

    void append(const T &newItem) {
        push_back(newItem);
    }

    void push_back(const T &newItem) {
        ASSERT(count() < _capacity);
        items[numItems++] = newItem;
    }

    void clear() {
        numItems = 0;
    }

    void erase(unsigned index) {
        // allow a.erase(a.find()) without checking for not-found.
        if (index == NOT_FOUND)
            return;

        erase(items + index);
    }

    void erase(iterator item) {
        ASSERT(item >= begin() && item < end());
        memcpy((uint8_t*)item, (uint8_t*)(item+1), (end() - item) - 1);
        numItems--;
    }

    iterator begin() {
        return &items[0];
    }

    iterator end() {
        return &items[count()];
    }
    
    unsigned find(T itemToFind) {
        int index = 0;
        for (iterator I=begin(), E=end(); I != E; ++I, ++index) {
            if (*I == itemToFind)
                return index;
        }
        return NOT_FOUND; // not found
    }

private:
    T items[_capacity];
    sizeT numItems;
};

};  // namespace Sifteo
