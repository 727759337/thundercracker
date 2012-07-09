#!/usr/bin/env pypy
#
# Playing with using Galois fields... We have a hardware 8-bit GF multiplier
# in the nRF chip, so we might as well use it for things. This is evaluating
# their feasibility for channel hopping and for flash CRCs.
#
# This is SUPER slow in the CPython intepreter. Using PyPy with JIT for speed.
#
# References:
#   http://www.samiam.org/galois.html
#   http://opencores.org/websvn,filedetails?repname=fast-crc&path=%2Ffast-crc%2Fweb_uploads%2FCRC_ie3_contest.pdf
#
# M. Elizabeth Scott <beth@sifteo.com>
#

import random
from hex import hexDump

# Honestly, with the 4-bit algorithm below, these are all pretty much equally good.
BEST_GEN = 0x8e

def gfm(a, b):
    # GF(2^8) multiplier, using the AES polynomial
    p = 0
    for bit in range(8):
        if b & 1:
            p = p ^ a
        msb = a & 0x80
        a = (a << 1) & 0xff
        if msb:
            a = a ^ 0x1b
        b = b >> 1
    return p

assert gfm(7,3) == 9
assert gfm(4,3) == 0xc
assert gfm(0xe5,0xe5) == 0x4c

gfmTables = {}

def tableGFM(a, b):
    if b not in gfmTables:
        gfmTables[b] = [gfm(x, b) for x in range(256)]
    return gfmTables[b][a]


def findNBitGenerators(n):
    # A Generator is any number which, when exponentiated, hits every number
    # in the field. This function exhaustively searches for generators in
    # a field which is an N-bit subset of the AES GF(2^8) field. Note that
    # the generator itself may be a full 8-bit number, i.e. the product is
    # truncated to N bits after multiplication.

    results = []
    sample = None

    for value in range(256):
        x = value
        l = []
        r = range(1, 1<<n)
        for i in r:
            x = gfm(x, value) & ((1<<n)-1)
            l.append(x)
        if list(sorted(l)) == list(r):
            results.append(value)
            sample = sample or l

    return results, sample

def findAllGenerators():
    for n in range(2,9):
        gen, sample = findNBitGenerators(n)
        print "\n%d-bit generators in the AES Galois Field:" % n
        hexDump(gen, prefix="\t")
        print "\nSample sequence:"
        hexDump(sample, prefix="\t")

def xcrc(byte, crc, gen=BEST_GEN):
    # Experimental CRC based on galois field multiplication
    return tableGFM(tableGFM(crc, gen) ^ (byte & 0x0F), gen) ^ (byte >> 4)

def crcList(l, gen=BEST_GEN):
    reg = 0
    for x in l:
        reg = xcrc(x, reg, gen)
    return reg

def testCRCBytes():
    # Try out our experimental CRC function!
    inputString = map(ord, "this is a test.....\0\0\0\0\0\1\1\1\1\1\2\2\2\2\2\3\3\3\3\3")
    crcString = []
    reg = 0
    for byte in inputString:
        reg = xcrc(byte, reg)
        crcString.append(reg)
    print "\nTest string:"
    hexDump(inputString, prefix="\t")
    print "\nCRC after each byte of test string:"
    hexDump(crcString, prefix="\t")

def bitErrorDetectionHistogram(gen, stringLen=256, numStrings=4):
    # Evaluate a given generator byte's ability to detect bit errors. Start with a random
    # string, and flip each bit. Take the CRC of each of these permutations, and calculate
    # a histogram of those CRCs. Returns that histogram.

    buckets = [0] * 256

    for n in range(numStrings):
        inputString = [random.randrange(256) for i in range(stringLen)]
        for byte in range(len(inputString)):
            for bit in range(8):
                permuted = list(inputString)
                permuted[byte] = permuted[byte] ^ (1 << bit)
                c = crcList(permuted, gen)
                buckets[c] = min(0xff, buckets[c] + 1)

    return buckets

def scoreHistogram(histogram):
    # Calculate a 'score' for how evenly distributed a histogram is
    # (Mean squared error vs. perfectly flat histogram)

    avg = sum(histogram) / float(len(histogram))
    err = 0.0
    for v in histogram:
        err += (v-avg)*(v-avg)
    return err / len(histogram)

def testCRCBitErrors():
    print "\nEvaluating bit-error detection for generators:"
    best = None
    for gen in findNBitGenerators(8)[0]:
        hist = bitErrorDetectionHistogram(gen)
        score = scoreHistogram(hist)
        print "\t%02x: Score %f" % (gen, score)
        if best is None or score < best[0]:
            best = (score, gen, hist)

    print "\nHistogram for best (%02x):" % best[1]
    hexDump(best[2], prefix="\t")


if __name__ == "__main__":
    findAllGenerators()
    testCRCBytes()
    testCRCBitErrors()
