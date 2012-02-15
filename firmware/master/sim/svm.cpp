#include "svm.h"
#include "elfdefs.h"
#include "flashlayer.h"

#include <string.h>

Svm::Svm()
{
}

SvmProgram::SvmProgram()
{
}

void SvmProgram::run()
{
    if (!loadElfFile(0, 12345))
        return;

    LOG(("loaded. entry: 0x%x text start: 0x%x\n", progInfo.entry, progInfo.textRodata.start));

    validate();

//    for () {
//        cycle();
//    }
}

void SvmProgram::cycle()
{
}

void SvmProgram::validate()
{
    unsigned fsize;

    void *block = FlashLayer::getRegionFromOffset(progInfo.textRodata.start + progInfo.entry, FlashLayer::BLOCK_SIZE, &fsize);
    if (!block) {
        LOG(("failed to get entry block\n"));
        return;
    }

    uint32_t *instr = static_cast<uint32_t*>(block);
    for (; fsize != 0; fsize -= sizeof(uint32_t), ++instr) {
        // if bits [15:11] are 0b11101, 0b11110 or 0b11111, it's a 32-bit instruction
        // otherwise it's 16-bit
        uint16_t high5bits = *instr & (0b11111 << 11);
        if (high5bits >= (0b11101 << 11)) {
            if (!isValid32(*instr)) {
                return;
            }
        }
        else {
            if (!isValid16(*instr & 0xFFFF))
                return;
            if (!isValid16((*instr >> 16) & 0xFFFF))
                return;
        }
    }
}

/*
Allowed 16-bit instruction encodings:

  00xxxxxx xxxxxxxx     lsl, lsr, asr, add, sub, mov, cmp
  010000xx xxxxxxxx     and, eor, lsl, lsr, asr, adc, sbc, ror,
                        tst, rsb, cmp, cmn, orr, mul, bic, mvn

  10110010 xxxxxxxx     uxth, sxth, uxtb, sxtb
  10111111 00000000     nop
  11011111 xxxxxxxx     svc

  01001xxx xxxxxxxx     ldr r0-r7, [PC, #imm8]
  1001xxxx xxxxxxxx     ldr/str r0-r7, [SP, #imm8]
  10101xxx xxxxxxxx     add r0-r7, SP, #imm8

Near branches are allowed, assuming the target address is within the code
region of the same page, and the target is 32-bit aligned:

  1011x0x1 xxxxxxxx     cb(n)z
  1101xxxx xxxxxxxx     bcc
  11100xxx xxxxxxxx     b
*/
bool SvmProgram::isValid16(uint16_t halfword)
{
    if ((halfword & AluMask) == AluTest) {
        // 00xxxxxx xxxxxxxx     lsl, lsr, asr, add, sub, mov, cmp
        LOG(("arithmetic\n"));
        return true;
    }
    if ((halfword & DataProcMask) == DataProcTest) {
        // 010000xx xxxxxxxx    and, eor, lsl, lsr, asr, adc, sbc, ror,
        //                      tst, rsb, cmp, cmn, orr, mul, bic, mvn
        LOG(("data processing\n"));
        return true;
    }
    if ((halfword & MiscMask) == MiscTest) {
        // 10110010 xxxxxxxx     uxth, sxth, uxtb, sxtb
        LOG(("miscellaneous\n"));
        return true;
    }
    if ((halfword & SvcMask) == SvcTest) {
        // 11011111 xxxxxxxx     svc
        LOG(("svc\n"));
        return true;
    }
    if ((halfword & PcRelLdrMask) == PcRelLdrTest) {
        // 01001xxx xxxxxxxx     ldr r0-r7, [PC, #imm8]
        LOG(("pc relative ldr\n"));
        return true;
    }
    if ((halfword & SpRelLdrStrMask) == SpRelLdrStrTest) {
        // 1001xxxx xxxxxxxx     ldr/str r0-r7, [SP, #imm8]
        LOG(("sp relative ldr/str\n"));
        return true;
    }
    if ((halfword & SpRelAddMask) == SpRelAddTest) {
        // 10101xxx xxxxxxxx     add r0-r7, SP, #imm8
        LOG(("sp relative add\n"));
        return true;
    }
    if ((halfword & UncondBranchMask) == UncondBranchTest) {
        // 11100xxx xxxxxxxx     b
        LOG(("unconditional branch\n"));
        // TODO: must validate target
        return true;
    }
    if ((halfword & CondBranchMask) == CondBranchTest) {
        // 1101xxxx xxxxxxxx     bcc
        LOG(("branchcc\n"));
        // TODO: must validate target
        return true;
    }
    if (halfword == Nop) {
        // 10111111 00000000     nop
        LOG(("nop\n"));
        return true;
    }
    LOG(("*********************************** invalid instruction: 0x%x\n", halfword));
    return false;
}

/*
Allowed 32-bit instruction encodings:

  11111000 11001001, 0xxxxxxx xxxxxxxx      str         r0-r7, [r9, #imm12]
  11111000 10x01001, 0xxxxxxx xxxxxxxx      str[bh]     r0-r7, [r9, #imm12]

  1111100x 10x1100x, 0xxxxxxx xxxxxxxx      ldr(s)[bh]  r0-r7, [r8-9, #imm12]
  11111000 1101100x, 0xxxxxxx xxxxxxxx      ldr         r0-r7, [r8-9, #imm12]

  11110x10 x100xxxx, 0xxx0xxx xxxxxxxx      mov[wt]     r0-r7, #imm16
*/
bool SvmProgram::isValid32(uint32_t inst)
{
    LOG(("----------------------- 32-bit instruction\n"));
    return true;
}

bool SvmProgram::loadElfFile(unsigned addr, unsigned len)
{
    unsigned sz;
    void *b = FlashLayer::getRegionFromOffset(addr, FlashLayer::BLOCK_SIZE, &sz);
    uint8_t *block = static_cast<uint8_t*>(b);

    // verify magicality
    if ((block[Elf::EI_MAG0] != Elf::Magic0) ||
        (block[Elf::EI_MAG1] != Elf::Magic1) ||
        (block[Elf::EI_MAG2] != Elf::Magic2) ||
        (block[Elf::EI_MAG3] != Elf::Magic3))
    {
        LOG(("incorrect elf magic number"));
        return false;
    }


    Elf::FileHeader header;
    memcpy(&header, block, sizeof header);

    // ensure the file is the correct Elf version
    if (header.e_ident[Elf::EI_VERSION] != Elf::EV_CURRENT) {
        LOG(("incorrect elf file version\n"));
        return false;
    }

    // ensure the file is the correct data format
    union {
        int32_t l;
        char c[sizeof (int32_t)];
    } u;
    u.l = 1;
    if ((u.c[sizeof(int32_t) - 1] + 1) != header.e_ident[Elf::EI_DATA]) {
        LOG(("incorrect elf data format\n"));
        return false;
    }

    if (header.e_machine != Elf::EM_ARM) {
        LOG(("elf: incorrect machine format\n"));
        return false;
    }

    progInfo.entry = header.e_entry;
    /*
        We're looking for 3 segments, identified by the following profiles:
        - text segment - executable & read-only
        - data segment - read/write & non-zero size on disk
        - bss segment - read/write, zero size on disk, and non-zero size in memory
    */
    unsigned offset = header.e_phoff;
    for (unsigned i = 0; i < header.e_phnum; ++i, offset += header.e_phentsize) {
        Elf::ProgramHeader pHeader;
        memcpy(&pHeader, block + offset, header.e_phentsize);
        if (pHeader.p_type != Elf::PT_LOAD)
            continue;

        switch (pHeader.p_flags) {
        case (Elf::PF_Exec | Elf::PF_Read):
            LOG(("rodata/text segment found\n"));
            progInfo.textRodata.start = pHeader.p_offset;
            progInfo.textRodata.size = pHeader.p_filesz;
            break;
        case (Elf::PF_Read | Elf::PF_Write):
            if (pHeader.p_memsz >= 0 && pHeader.p_filesz == 0) {
                LOG(("bss segment found\n"));
                progInfo.bss.start = pHeader.p_offset;
                progInfo.bss.size = pHeader.p_memsz;
            }
            else if (pHeader.p_filesz > 0) {
                LOG(("found rwdata segment\n"));
                progInfo.rwdata.start = pHeader.p_offset;
                progInfo.rwdata.size = pHeader.p_memsz;
            }
            break;
        }
    }

    FlashLayer::releaseRegionFromOffset(addr);
    return true;
}
