/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker launcher
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

#pragma once
#include <sifteo.h>
#include "mainmenuitem.h"


/**
 * The NineBlock pattern is based on a classic quilting pattern, using
 * a concept borrowed from Identicon: we can generate a visually distinctive
 * icon that corresponds with a hash.
 *
 * Our NineBlock pattern has a fixed center square (for displaying a mini-icon)
 * and a fixed color (since the assets must be pre-baked). Still, this gives us
 * nearly 1024 different icons.
 */

class NineBlock
{
public:

    /**
     * Generate a 12x12 tile image based on the 'identity' value.
     * This value is used as the seed for a PRNG that generates the
     * characteristics of this icon.
     */
    static void generate(unsigned identity, MainMenuItem::IconBuffer &icon);

private:

    /// Generate an asset frame number
    static unsigned frame(unsigned pattern, unsigned angle, bool corner);
};
