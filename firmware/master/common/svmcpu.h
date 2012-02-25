#ifndef SVMCPU_H
#define SVMCPU_H

#include "svm.h"

using namespace Svm;

class SvmRuntime;

class SvmCpu {
public:
    void init(SvmRuntime *runtime);
    void run();

    reg_t call(reg_t address);

    /*
        For now, register accessors are assumed to only really be used to access
        stacked registers during interrupt handling.
    */
    reg_t reg(uint8_t r) const;
    void setReg(uint8_t r, uint32_t val);

    // address of user ram
    reg_t userRam() const;

    static const unsigned NUM_REGS = 16;
    static const unsigned REG_PC = 15;
    static const unsigned REG_LR = 14;
    static const unsigned REG_SP = 13;
    static const unsigned REG_FP = 11;

    static const unsigned MEM_IN_BYTES = 32 * 1024;

#ifdef SIFTEO_SIMULATOR
    inline void BranchOffsetPC(int offset) {
        regs[REG_PC] = (regs[REG_PC] + offset + 2) & ~(reg_t)1;
    }
#endif

private:
#if 0
    // this gets saved at interrupt by HW on cortex-m3
    struct HwStackFrame {
        reg_t r0;
        reg_t r1;
        reg_t r2;
        reg_t r3;
        reg_t r12;
        reg_t lr;   // link register
        reg_t pc;   // program counter
        reg_t psr;  // program status register
    };

    // the rest of the stack that must be saved by software
    struct SwStackFrame {
        reg_t r4;
        reg_t r5;
        reg_t r6;
        reg_t r7;
        reg_t r8;
        reg_t r9;
        reg_t r10;
        reg_t r11;
    };
#endif

    SvmRuntime *runtime;

#ifdef SIFTEO_SIMULATOR
    reg_t regs[NUM_REGS];
    reg_t cpsr;
    uint8_t mem[MEM_IN_BYTES];

    // ARM emulation support
    uint16_t fetch();
    void execute16(uint16_t instr);
    void execute32(uint32_t instr);
    bool conditionPassed(uint8_t cond);

    enum Conditions {
        EQ = 0,    // Equal
        NE = 1,    // Not Equal
        CS = 2,    // Carry Set
        CC = 3,    // Carry Clear
        MI = 4,    // Minus, Negative
        PL = 5,    // Plus, positive, or zero
        VS = 6,    // Overflow
        VC = 7,    // No overflow
        HI = 8,    // Unsigned higher
        LS = 9,    // Unsigned lower or same
        GE = 10,    // Signed greater than or equal
        LT = 11,    // Signed less than
        GT = 12,    // Signed greater than
        LE = 13,    // Signed less than or equal
        NoneAL = 14 // None, always, unconditional
    };

    // status flag helpers
    inline bool getNeg() const {
        return cpsr & (1 << 31);
    }
    inline void setNeg() {
        cpsr |= (1 << 31);
    }
    inline void clrNeg() {
        cpsr &= ~(1 << 31);
    }

    inline bool getZero() const {
        return cpsr & (1 << 30);
    }
    inline void setZero() {
        cpsr |= 1 << 30;
    }
    inline void clrZero() {
        cpsr &= ~(1 << 30);
    }

    inline bool getCarry() const {
        return cpsr & (1 << 29);
    }
    inline void setCarry() {
        cpsr |= 1 << 29;
    }
    inline void clrCarry() {
        cpsr &= ~(1 << 29);
    }

    inline int getOverflow() const {
        return cpsr & (1 << 28);
    }
    inline void setOverflow() {
        cpsr |= (1 << 28);
    }
    inline void clrOverflow() {
        cpsr &= ~(1 << 28);
    }

    // shift/add/subtract/move/compare
    void emulateLSLImm(uint16_t inst);      // LSL (immediate)
    void emulateLSRImm(uint16_t inst);      // LSR (immediate)
    void emulateASRImm(uint16_t instr);     // ASR (immediate)
    void emulateADDReg(uint16_t instr);     // ADD (register)
    void emulateSUBReg(uint16_t instr);     // SUB (register)
    void emulateADD3Imm(uint16_t instr);    // ADD (immediate)
    void emulateSUB3Imm(uint16_t instr);    // SUB (immediate)
    void emulateMovImm(uint16_t instr);     // MOV (immediate)
    void emulateCmpImm(uint16_t instr);     // CMP (immediate)
    void emulateADD8Imm(uint16_t instr);    // ADD (immediate)
    void emulateSUB8Imm(uint16_t instr);    // SUB (immediate)

    // data processing
    void emulateANDReg(uint16_t instr);     // AND (register)
    void emulateEORReg(uint16_t instr);     // EOR (register)
    void emulateLSLReg(uint16_t instr);     // LSL (register)
    void emulateLSRReg(uint16_t instr);     // LSR (register)
    void emulateASRReg(uint16_t instr);     // ASR (register)
    void emulateADCReg(uint16_t instr);     // ADC (register)
    void emulateSBCReg(uint16_t instr);     // SBC (register)
    void emulateRORReg(uint16_t instr);     // ROR (register)
    void emulateTSTReg(uint16_t instr);     // TST (register)
    void emulateRSBImm(uint16_t instr);     // RSB (immediate)
    void emulateCMPReg(uint16_t instr);     // CMP (register)
    void emulateCMNReg(uint16_t instr);     // CMN (register)
    void emulateORRReg(uint16_t instr);     // ORR (register)
    void emulateMUL(uint16_t instr);        // MUL
    void emulateBICReg(uint16_t instr);     // BIC (register)
    void emulateMVNReg(uint16_t instr);     // MVN (register)

    // misc
    void emulateSXTH(uint16_t instr);       // SXTH
    void emulateSXTB(uint16_t instr);       // SXTB
    void emulateUXTH(uint16_t instr);       // UXTH
    void emulateUXTB(uint16_t instr);       // UXTB

    void emulateB(uint16_t instr);          // B
    void emulateCondB(uint16_t instr);      // B
    void emulateCBZ_CBNZ(uint16_t instr);   // CBNZ, CBZ

    void emulateSTRSPImm(uint16_t instr);   // STR (SP plus immediate)
    void emulateLDRSPImm(uint16_t instr);   // LDR (SP plus immediate)
    void emulateADDSpImm(uint16_t instr);   // ADD (SP plus immediate)

    void emulateLDRLitPool(uint16_t instr); // LDR (literal)

    void emulateSVC(uint16_t instr);

    // 32-bit instructions
    void emulateSTR(uint32_t instr);
    void emulateSTRBH(uint32_t instr);
    void emulateLDRBH(uint32_t instr);
    void emulateLDR(uint32_t instr);
    void emulateMOVWT(uint32_t instr);
    void emulateDIV(uint32_t instr);

    // utils
    uint32_t ROR(uint32_t data, uint32_t ror) {
        return (data << (32 - ror)) | (data >> ror);
    }

    template <typename T>
    inline T Align(T x, T y) {
        return y * (x / y);
    }

    // http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    template <typename T, unsigned B>
    inline T SignExtend(const T x) {
        struct { T x:B; } s;
        return s.x = x;
    }
#endif
};

#endif // SVMCPU_H
