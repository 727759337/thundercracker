
#include "elfdebuginfo.h"
#include <cxxabi.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/*
 * NOTE: at the moment, this is a slightly modified version of
 * emulator/src/mc_elfdebuginfo.cpp Eventually want to migrate that out of
 * the emulator, and make this available as library code to be shared by
 * both swiss and the emulator (if both end up wanting to use it).
 */

void ELFDebugInfo::clear()
{
    program = 0;
    sections.clear();
    sectionMap.clear();
}

bool ELFDebugInfo::copyProgramBytes(uint32_t byteOffset, uint8_t *dest, uint32_t length) const
{
    /*
     * Copy data out of the program's ELF
     */
    if (!program)
        return false;

    // TODO: mmap?
    if (fseek(program, byteOffset, SEEK_SET) != 0)
        return false;

    if (fread(dest, 1, length, program) != length)
        return false;

    return true;
}

bool ELFDebugInfo::init(const char *elfPath)
{
    /*
     * Build a table of debug sections.
     */

    clear();
    program = fopen(elfPath, "rb");
    if (!program)
        return false;

    Elf::FileHeader header;
    if (!copyProgramBytes(0, (uint8_t*)&header, sizeof header))
        return false;

    // First pass, copy out all the headers.
    for (unsigned i = 0; i < header.e_shnum; i++) {
        sections.push_back(Elf::SectionHeader());
        Elf::SectionHeader *pHdr = &sections.back();
        uint32_t off = header.e_shoff + i * header.e_shentsize;
        if (!copyProgramBytes(off, (uint8_t*)pHdr, sizeof *pHdr))
            memset(pHdr, 0xFF, sizeof *pHdr);
    }

    // Read section names, set up sectionMap.
    if (header.e_shstrndx < sections.size()) {
        const Elf::SectionHeader *strTab = &sections[header.e_shstrndx];
        for (unsigned i = 0, e = sections.size(); i != e; ++i) {
            Elf::SectionHeader *pHdr = &sections[i];
            sectionMap[readString(strTab, pHdr->sh_name)] = pHdr;
        }
    }
}

const Elf::SectionHeader *ELFDebugInfo::findSection(const std::string &name) const
{
    sectionMap_t::const_iterator I = sectionMap.find(name);
    if (I == sectionMap.end())
        return 0;
    else
        return I->second;
}

std::string ELFDebugInfo::readString(const Elf::SectionHeader *SI, uint32_t offset) const
{
    static char buf[1024];
    uint32_t size = std::min<uint32_t>(sizeof buf - 1, SI->sh_size - offset);
    if (!copyProgramBytes(SI->sh_offset + offset, (uint8_t*)buf, size))
        size = 0;
    buf[size] = '\0';
    return buf;
}

std::string ELFDebugInfo::readString(const std::string &section, uint32_t offset) const
{
    const Elf::SectionHeader *SI = findSection(section);
    if (SI)
        return readString(SI, offset);
    else
        return "";
}

bool ELFDebugInfo::findNearestSymbol(uint32_t address,
    Elf::Symbol &symbol, std::string &name) const
{
    // Look for the nearest ELF symbol which contains 'address'.
    // If we find anything, returns 'true' and leaves the symbol information
    // in "symbol", and the name is read into the "name" buffer.
    //
    // If nothing is found, we still fill in the output buffer and name
    // with "(unknown)" and zeroes, but 'false' is returned.

    const Elf::SectionHeader *SI = findSection(".symtab");
    if (SI) {
        Elf::Symbol currentSym;
        const uint32_t worstOffset = (uint32_t) -1;
        uint32_t bestOffset = worstOffset;

        for (unsigned index = 0;; index++) {
            uint32_t tableOffset = index * sizeof currentSym;

            if (tableOffset + sizeof currentSym > SI->sh_size ||
                !copyProgramBytes(
                    SI->sh_offset + tableOffset,
                    (uint8_t*) &currentSym, sizeof currentSym))
                break;

            // Strip the Thumb bit from function symbols.
            if ((currentSym.st_info & 0xF) == Elf::STT_FUNC)
                currentSym.st_value &= ~1;

            uint32_t addrOffset = address - currentSym.st_value;
            if (addrOffset < currentSym.st_size && addrOffset < bestOffset) {
                symbol = currentSym;
                bestOffset = addrOffset;
            }
        }

        if (bestOffset != worstOffset) {
            name = readString(".strtab", symbol.st_name);
            return true;
        }
    }

    memset(&symbol, 0, sizeof symbol);
    name = "(unknown)";
    return false;
}

std::string ELFDebugInfo::formatAddress(uint32_t address) const
{
    Elf::Symbol symbol;
    std::string name;

    findNearestSymbol(address, symbol, name);
    demangle(name);
    uint32_t offset = address - symbol.st_value;

    if (offset != 0) {
        char buf[16];
        snprintf(buf, sizeof buf, "+0x%x", offset);
        name += buf;
    }

    return name;
}

void ELFDebugInfo::demangle(std::string &name)
{
    // This uses the demangler built into GCC's libstdc++.
    // It uses the same name mangling style as clang.

    int status;
    char *result = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
    if (status == 0) {
        name = result;
        free(result);
    }
}

bool ELFDebugInfo::readROM(uint32_t address, uint8_t *buffer, uint32_t bytes) const
{
    /*
     * Try to read from read-only data in the ELF file. If the read can be
     * satisfied locally, from the ELF data, returns true and fills in 'buffer'.
     * If not, returns false.
     */

    for (sections_t::const_iterator I = sections.begin(), E = sections.end(); I != E; ++I) {

        // Section must be allocated program data
        if (I->sh_type != Elf::SHT_PROGBITS)
            continue;
        if (!(I->sh_flags & Elf::SHF_ALLOC))
            continue;

        // Section must not be writeable
        if (I->sh_flags & Elf::SHF_WRITE)
            continue;

        // Address range must be fully contained within the section
        uint32_t offset = address - I->sh_addr;
        if (offset > I->sh_size || offset + bytes > I->sh_size)
            continue;

        // Success, we can read from the ELF
        return copyProgramBytes(offset, buffer, bytes);
    }

    return false;
}
