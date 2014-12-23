#!/usr/bin/env python
#
# Simple static binary translator for our 8051 firmware. This is used
# to "bake in" a stable firmware image, so that it isn't necessary to
# have an actual cube firmware ROM in order to run the emulator.
#
# This is done to an extent for performance, but also really for code
# security. It's much, much harder to reverse engineer` a usable ROM
# image from the siftulator binary due to this transformation.
#
# Since we need some hints about basic blocks and data vs. code, we
# take the SDCC ".rst" files as input, rather than the raw hex file.
#
# M. Elizabeth Scott <beth@sifteo.com>
# 
# Copyright (c) 2011 Sifteo, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

import FirmwareLib
import sys
import bin2c


def fixupImage(p):
    """This contains all the special-case code we apply to the compiled
       firmware during binary translation.
       """

    # Our ROM palettes are actually generated machine code that
    # is jumped to when we set each palette. These never existed
    # as assembly code in the first place, so we get no mention
    # of them in the .rst file other than as data. So, we need to
    # manually annotate these.
    # 
    # Each palette is 16 bytes, consisting of 8 two-byte opcodes.

    paletteBase = p.symbols['_rom_palettes']
    for palNumber in range(16):
        addr = paletteBase + palNumber * 16
        p.branchTargets[addr] = True
        for instr in range(8):
            instrBase = addr + instr * 2
            p.instructions[instrBase] = [p.dataMemory[instrBase : instrBase+2]]


class CodeGenerator:
    def __init__(self, parser):
        self.p = parser
        self.opTable = FirmwareLib.opcodeTable()

    def write(self, f):
        f.write(bin2c.HEADER)
        f.write("#include <cube_cpu_opcodes.h>\n"
                "\n"
                "namespace Cube {\n"
                "namespace CPU {\n"
                "\n"
                "static int FASTCALL sbt_exception(em8051 *aCPU) {\n"
                "\texcept(aCPU, EXCEPTION_SBT);\n"
                "\treturn 1;\n"
                "}\n")

        self.writeCode(f)
        bin2c.writeArray(f, 'sbt_rom_data', self.p.dataMemory)

        f.write("};  // namespace CPU\n"
                "};  // namespace Cube\n")

    def beginBlock(self, f, addr):
        f.write("\nstatic int FASTCALL sbt_block_%04x(em8051 *aCPU)\n"
                "{\n"
                "\tunsigned clk = 0;\n"
                "\tunsigned pc = 0x%04x;\n"
                % (addr, addr))

    def endBlock(self, f):
        f.write("\taCPU->mPC = pc & PC_MASK;\n"
                "\treturn clk;\n"
                "}\n")
        
    def writeInstruction(self, f, bytes):
        bytes += (0, 0)
        f.write("\tclk += Opcodes::%-20s(aCPU, pc, 0x%02x,0x%02x,0x%02x);\n" % (
                self.opTable[bytes[0]], bytes[0], bytes[1], bytes[2]))

    def endsBlock(self, bytes):
        """Does this instruction need to end a translation block?"""
        mnemonic = self.opTable[bytes[0]]

        # If it changes control flow, we exit.
        # (Cheesy jump detector!)

        if 'j' in mnemonic or 'call' in mnemonic or 'ret' in mnemonic:
            return True

        # Normally SFR writes won't trigger an end-of-block, but there are
        # some operations that really need to come back to earth after each
        # SFR write. One of these is neighbors, since we need the pulses to
        # propagate instantly across all cubes. We also do this for I2C, since
        # the I2C state machine is driven exclusively by the tick timer.

        if 'mov_mem' in mnemonic:
            dest = bytes[1]
            if dest in (

                # Neighbors
                0x90,  # P1
                0x94,  # P1DIR

                # I2C
                0xDA,  # W2DAT
                0xE1,  # W2CON1
                0xE2,  # W2CON0
                
                # RF SPI
                0xE7,  # SPIRDAT

                # Power
                0xAD,  # CLKLFCTRL
                0xAF,  # WDSV

                ):
                return True

        return False

    def writeCode(self, f):
        addrs = self.p.instructions.keys()
        addrs.sort()
        inBlock = False
        blockMap = {}

        # Walk our instruction table, and emit translation blocks as functions

        for addr in addrs:

            # Branch targets end a basic block before emitting the instruction
            if addr in self.p.branchTargets and inBlock:
                self.endBlock(f)
                inBlock = False

            if not inBlock:
                self.beginBlock(f, addr)
                blockMap[addr] = True
                inBlock = True

            # Normally there's one instruction per address, but patches can put
            # as many instructions as they like at the same place.
            for bytes in self.p.instructions[addr]:
                self.writeInstruction(f, bytes)
                endsBlock = self.endsBlock(bytes)

            if endsBlock and inBlock:
                self.endBlock(f)
                inBlock = False

        if inBlock:
            self.endBlock(f)

        # Write a table of translated block functions

        f.write("const sbt_block_t sbt_rom_code[] = {\n")

        for addr in range(FirmwareLib.ROM_SIZE):
            if addr in blockMap:
                f.write("\t&sbt_block_%04x,\n" % addr)
            else:
                f.write("\t&sbt_exception,\n")

        f.write("};\n")


if __name__ == '__main__':
    p = FirmwareLib.RSTParser()
    for f in sys.argv[1:]:
        p.parseFile(f)

    fixupImage(p)
    gen = CodeGenerator(p)
    gen.write(open('resources/firmware-sbt.cpp', 'w'))
