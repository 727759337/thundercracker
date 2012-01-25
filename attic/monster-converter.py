#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Script to fetch monster data from tacolab.com, and format it
# for Thundercracker's VRAM. (4-bit color, 32x32, with trailing
# RGB565 palette)
#
# M. Elizabeth Scott <beth@sifteo.com>
#
# Copyright <c> 2011 Sifteo, Inc. All rights reserved.
#

import urllib2, re, cStringIO
import Image

def RGB565(r, g, b):
    #
    # Round to the nearest 5/6 bit color. Note that simple
    # bit truncation does NOT produce the best result!
    #
    r5 = (r * 31 + 128) // 255;
    g6 = (g * 63 + 128) // 255;
    b5 = (b * 31 + 128) // 255;
    return (r5 << 11) | (g6 << 5) | b5

def RL4(x):
    # 4-bit left rotate
    return ((x << 1) | (x >> 3)) & 0xF

def RR4(x):
    # 4-bit right rotate
    return ((x << 3) | (x >> 1)) & 0xF


def convert(number, name, data):
    orig = Image.open(cStringIO.StringIO(data)).convert("RGBA")

    # White background
    bg = Image.new("RGBA", orig.size)
    bg.paste((255, 255, 255, 255))

    # Convert to RGB, without alpha
    im = Image.composite(orig, bg, orig).convert("RGB")

    # Make a 16-color palette
    im = im.quantize(16)

    print """
// #%d -- %s
static const struct MonsterData %s = {""" % (number, name, name)

    for y in range(32):
        print "    ",
        for x in range(32):

            # Sample in the center of each pixel
            sx = (x - 0.5) * im.size[0] / 30
            sy = (y - 0.5) * im.size[1] / 30
            if sx >= 0 and sy >= 0 and sx < im.size[0] and sy < im.size[1]:
                index = im.getpixel((sx, sy))
            else:
                index = 0

            # Palette swizzle: Use even indices first, since those compress much better
            index = RL4(index)

            if x & 1:
                byte = nybble | (index << 4)
                print "0x%02x," % byte,
            else:
                nybble = index
        print

    palette = im.getpalette()
    for i in range(16):
        if (i % 8) == 0:
            print "    ",
    
        # Inverse palette swizzle
        offset = RR4(i) * 3
            
        rgb = RGB565(*palette[offset:offset+3])

        print "0x%02x,0x%02x," % (rgb & 0xFF, rgb >> 8),
        if (i % 8) == 7:
            print

    print "};"


def main():

    print """/*
 * Prof. Engd's Étagère of Hypnopompic Hexadecimal Miniature Monsters
 * http://monster.tacolab.com
 *
 * Copyright <c> 2009 Taco Lab LLC.
 * Licensed under a Creative Commons license:
 * http://creativecommons.org/licenses/by-nc-sa/3.0/us/
 */

#ifndef _MONSTER_H
#define _MONSTER_H

#include <sifteo.h>

struct MonsterData {
    uint8_t fb[512 + 32];
};"""

    monsterList = []
    archive = urllib2.urlopen("http://monster.tacolab.com/monster/archive").read()
    for match in re.finditer(r"/images/monsters/0000/(....)/(\S+)_thumb.png", archive):
        number = int(match.group(1))
        name = match.group(2)
        data = urllib2.urlopen("http://monster.tacolab.com/images/monsters/0000/%04d/%s.png"
                               % (number, name)).read()

        name = name.replace('-','_')
        monsterList.append(name)
        convert(number, name, data)

    print "\nstatic const struct MonsterData* monsters[] = {"
    for name in monsterList:
        print "    &%s," % name
    print """};

#endif
"""


if __name__ == "__main__":
    main()

