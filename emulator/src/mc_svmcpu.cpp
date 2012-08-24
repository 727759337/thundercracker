/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "svmcpu.h"
#include "svmruntime.h"
#include "macros.h"
#include "machine.h"
#include "mc_timing.h"
#include "system.h"
#include "system_mc.h"
#include "svmmemory.h"

#include <string.h>

namespace SvmCpu {

static reg_t regs[NUM_REGS];
UserRegs userRegs;


/***************************************************************************
 * Timing
 ***************************************************************************/

/*
 * First-level cycle counter. During instruction emulation, we increment this
 * as appropriate. Periodically (on every taken branch or SVC, currently)
 * we forward any whole clock ticks to SystemMC::elapseTicks() via
 * calculateElapsedTicks().
 *
 * Values in here are pre-multiplied by the CPU_RATE_DENOMINATOR. To convert
 * to system ticks, we just need to divide by CPU_RATE_NUMERATOR and save
 * the remainder.
 */
static unsigned svmCyclesElapsed;

static void calculateElapsedTicks()
{
    unsigned elapsed = svmCyclesElapsed;
    if (elapsed >= MCTiming::CPU_THRESHOLD) {
        unsigned ticks = elapsed / MCTiming::CPU_RATE_NUMERATOR;
        svmCyclesElapsed = elapsed % MCTiming::CPU_RATE_NUMERATOR;
        SystemMC::elapseTicks(ticks);
    }
}


/***************************************************************************
 * Flags and Condition Codes
 ***************************************************************************/

static inline bool getNeg() {
    return Svm::getNeg(regs[REG_CPSR]);
}
static inline void setNeg(bool f) {
    if (f)
        regs[REG_CPSR] |= (1 << 31);
    else
        regs[REG_CPSR] &= ~(1 << 31);
}

static inline bool getZero() {
    return Svm::getZero(regs[REG_CPSR]);
}
static inline void setZero(bool f) {
    if (f)
        regs[REG_CPSR] |= 1 << 30;
    else
        regs[REG_CPSR] &= ~(1 << 30);
}

static inline bool getCarry() {
    return Svm::getCarry(regs[REG_CPSR]);
}
static inline void setCarry(bool f) {
    if (f)
        regs[REG_CPSR] |= 1 << 29;
    else
        regs[REG_CPSR] &= ~(1 << 29);
}

static inline int getOverflow() {
    return Svm::getOverflow(regs[REG_CPSR]);
}
static inline void setOverflow(bool f) {
    if (f)
        regs[REG_CPSR] |= (1 << 28);
    else
        regs[REG_CPSR] &= ~(1 << 28);
}

static inline void setNZ(int32_t result) {
    setNeg(result < 0);
    setZero(result == 0);
}

static inline reg_t opLSL(reg_t a, reg_t b) {
    // Note: Intentionally truncates to 32-bit
    setCarry(b ? ((0x80000000 >> (b - 1)) & a) != 0 : 0);
    uint32_t result = b < 32 ? a << b : 0;
    setNZ(result);
    return result;
}

static inline reg_t opLSR(reg_t a, reg_t b) {
    // Note: Intentionally truncates to 32-bit
    setCarry(b ? ((1 << (b - 1)) & a) != 0 : 0);
    uint32_t result = b < 32 ? a >> b : 0;
    setNZ(result);
    return result;
}

static inline reg_t opASR(reg_t a, reg_t b) {
    // Note: Intentionally truncates to 32-bit
    setCarry(b ? ((1 << (b - 1)) & a) != 0 : 0);
    uint32_t result = b < 32 ? (int32_t)a >> b : 0;
    setNZ(result);
    return result;
}

static inline reg_t opADD(reg_t a, reg_t b, reg_t carry) {
    // Based on AddWithCarry() in the ARMv7 ARM, page A2-8
    uint64_t uSum32 = (uint64_t)(uint32_t)a + (uint32_t)b + (uint32_t)carry;
    int64_t sSum32 = (int64_t)(int32_t)a + (int32_t)b + (uint32_t)carry;
    setNZ(sSum32);
    setOverflow((int32_t)sSum32 != sSum32);
    setCarry((uint32_t)uSum32 != uSum32);

    // Preserve full reg_t width in result, even though we use 32-bit value for flags
    return a + b + carry;
}

static inline reg_t opAND(reg_t a, reg_t b) {
    reg_t result = a & b;
    setNZ(result);
    return result;
}

static inline reg_t opEOR(reg_t a, reg_t b) {
    reg_t result = a ^ b;
    setNZ(result);
    return result;
}


/***************************************************************************
 * Exception Handling
 ***************************************************************************/

/*
 * Only vaguely faithful exception emulation :)
 * Assume we're always in User mode & accessing the User stack pointer.
 *
 * cortex-m3 pushes a HwContext to the appropriate stack, User or Main,
 * to allow C-language interrupt handlers to be AAPCS compliant.
 */
static void emulateEnterException(reg_t returnAddr)
{
    // TODO: verify that we're not stacking to invalid memory
    regs[REG_SP] -= sizeof(HwContext);
    HwContext *ctx = reinterpret_cast<HwContext*>(regs[REG_SP]);
    ctx->r0         = regs[0];
    ctx->r1         = regs[1];
    ctx->r2         = regs[2];
    ctx->r3         = regs[3];
    ctx->r12        = regs[12];
    ctx->lr         = regs[REG_LR];
    ctx->returnAddr = returnAddr;
    ctx->xpsr       = regs[REG_CPSR];   // XXX; must also or in frameptralign

    ASSERT((ctx->returnAddr & 1) == 0 && "ReturnAddress from exception must be halfword aligned");

    regs[REG_LR] = 0xfffffffd;  // indicate that we're coming from User mode, using User stack

    userRegs.sp = regs[REG_SP];
}

/*
 * Pop HW context
 */
static void emulateExitException()
{
    regs[REG_SP] = userRegs.sp;

    HwContext *ctx = reinterpret_cast<HwContext*>(regs[REG_SP]);
    regs[0]         = ctx->r0;
    regs[1]         = ctx->r1;
    regs[2]         = ctx->r2;
    regs[3]         = ctx->r3;
    regs[12]        = ctx->r12;
    regs[REG_LR]    = ctx->lr;
    regs[REG_CPSR]  = ctx->xpsr;

    regs[REG_SP] += sizeof(HwContext);

    regs[REG_PC]    = ctx->returnAddr;
}

static void saveUserRegs()
{
    HwContext *ctx = reinterpret_cast<HwContext*>(userRegs.sp);
    memcpy(&userRegs.hw, ctx, sizeof *ctx);

    userRegs.irq.r4 = regs[4];
    userRegs.irq.r5 = regs[5];
    userRegs.irq.r6 = regs[6];
    userRegs.irq.r7 = regs[7];
    userRegs.irq.r8 = regs[8];
    userRegs.irq.r9 = regs[9];
    userRegs.irq.r10 = regs[10];
    userRegs.irq.r11 = regs[11];
}

static void restoreUserRegs()
{
    HwContext *ctx = reinterpret_cast<HwContext*>(userRegs.sp);
    memcpy(ctx, &userRegs.hw, sizeof *ctx);

    regs[4] = userRegs.irq.r4;
    regs[5] = userRegs.irq.r5;
    regs[6] = userRegs.irq.r6;
    regs[7] = userRegs.irq.r7;
    regs[8] = userRegs.irq.r8;
    regs[9] = userRegs.irq.r9;
    regs[10] = userRegs.irq.r10;
    regs[11] = userRegs.irq.r11;
}

static void emulateSVC(uint16_t instr)
{
    reg_t nextInstruction = regs[REG_PC];    // already incremented in fetch()
    emulateEnterException(nextInstruction);
    saveUserRegs();

    uint8_t imm8 = instr & 0xff;
    SvmRuntime::svc(imm8);

    restoreUserRegs();
    emulateExitException();
    calculateElapsedTicks();

    SystemMC::elapseTicks(MCTiming::TICKS_PER_SVC);
}

static void emulateFault(FaultCode code)
{
    /*
     * Emulate a fault exception. Faults are exceptions, just like SVCs,
     * so we need to simulate them in the same way before passing on the
     * fault code to SvmRuntime.
     *
     * Note that all instruction emulations need to explicitly abort the
     * current instruction after raising a fault. This function does not
     * alter the caller's control flow.
     */

    reg_t nextInstruction = regs[REG_PC];    // already incremented in fetch()
    emulateEnterException(nextInstruction);
    saveUserRegs();

    // Faults occur inside exception context too, just like SVCs.
    SvmRuntime::fault(code);

    restoreUserRegs();
    emulateExitException();
}


/***************************************************************************
 * Instruction Emulation
 ***************************************************************************/

// left shift
static void emulateLSLImm(uint16_t inst)
{
    unsigned imm5 = (inst >> 6) & 0x1f;
    unsigned Rm = (inst >> 3) & 0x7;
    unsigned Rd = inst & 0x7;

    regs[Rd] = opLSL(regs[Rm], imm5);
}

static void emulateLSRImm(uint16_t inst)
{
    unsigned imm5 = (inst >> 6) & 0x1f;
    unsigned Rm = (inst >> 3) & 0x7;
    unsigned Rd = inst & 0x7;

    if (imm5 == 0)
        imm5 = 32;

    regs[Rd] = opLSR(regs[Rm], imm5);
}

static void emulateASRImm(uint16_t instr)
{
    unsigned imm5 = (instr >> 6) & 0x1f;
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    if (imm5 == 0)
        imm5 = 32;

    regs[Rd] = opASR(regs[Rm], imm5);
}

static void emulateADDReg(uint16_t instr)
{
    unsigned Rm = (instr >> 6) & 0x7;
    unsigned Rn = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = opADD(regs[Rn], regs[Rm], 0);
}

static void emulateSUBReg(uint16_t instr)
{
    unsigned Rm = (instr >> 6) & 0x7;
    unsigned Rn = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = opADD(regs[Rn], ~regs[Rm], 1);
}

static void emulateADD3Imm(uint16_t instr)
{
    reg_t imm3 = (instr >> 6) & 0x7;
    unsigned Rn = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = opADD(regs[Rn], imm3, 0);
}

static void emulateSUB3Imm(uint16_t instr)
{
    reg_t imm3 = (instr >> 6) & 0x7;
    unsigned Rn = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = opADD(regs[Rn], ~imm3, 1);
}

static void emulateMovImm(uint16_t instr)
{
    unsigned Rd = (instr >> 8) & 0x7;
    unsigned imm8 = instr & 0xff;

    regs[Rd] = imm8;
}

static void emulateCmpImm(uint16_t instr)
{
    unsigned Rn = (instr >> 8) & 0x7;
    reg_t imm8 = instr & 0xff;

    reg_t result = opADD(regs[Rn], ~imm8, 1);
}

static void emulateADD8Imm(uint16_t instr)
{
    unsigned Rdn = (instr >> 8) & 0x7;
    reg_t imm8 = instr & 0xff;

    regs[Rdn] = opADD(regs[Rdn], imm8, 0);
}

static void emulateSUB8Imm(uint16_t instr)
{
    unsigned Rdn = (instr >> 8) & 0x7;
    reg_t imm8 = instr & 0xff;

    regs[Rdn] = opADD(regs[Rdn], ~imm8, 1);
}

///////////////////////////////////
// D A T A   P R O C E S S I N G
///////////////////////////////////

static void emulateANDReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = opAND(regs[Rdn], regs[Rm]);
}

static void emulateEORReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = opEOR(regs[Rdn], regs[Rm]);
}

static void emulateLSLReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    unsigned shift = regs[Rm] & 0xff;
    regs[Rdn] = opLSL(regs[Rdn], shift);
}

static void emulateLSRReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    unsigned shift = regs[Rm] & 0xff;
    regs[Rdn] = opLSR(regs[Rdn], shift);
}

static void emulateASRReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    unsigned shift = regs[Rm] & 0xff;
    regs[Rdn] = opASR(regs[Rdn], shift);
}

static void emulateADCReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = opADD(regs[Rdn], regs[Rm], getCarry());
}

static void emulateSBCReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = opADD(regs[Rdn], ~regs[Rm], getCarry());
}

static void emulateRORReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = Intrinsic::ROR(regs[Rdn], regs[Rm]);
}

static void emulateTSTReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    opAND(regs[Rdn], regs[Rm]);
}

static void emulateRSBImm(uint16_t instr)
{
    unsigned Rn = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = opADD(~regs[Rn], 0, 1);
}

static void emulateCMPReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;
    
    opADD(regs[Rdn], ~regs[Rm], 1);
}

static void emulateCMNReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    opADD(regs[Rdn], regs[Rm], 0);
}

static void emulateORRReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    reg_t result = regs[Rdn] | regs[Rm];
    regs[Rdn] = result;
    setNZ(result);
}

static void emulateMUL(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    uint64_t result = (uint64_t)regs[Rdn] * (uint64_t)regs[Rm];
    regs[Rdn] = (uint32_t) result;

    // Flag calculations always use the full 64-bit result
    setNeg((int64_t)result < 0);
    setZero(result == 0);
}

static void emulateBICReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = (uint32_t) (regs[Rdn] & ~(regs[Rm]));
}

static void emulateMVNReg(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = (uint32_t) ~regs[Rm];
}

/////////////////////////////////////
// M I S C   I N S T R U C T I O N S
/////////////////////////////////////

static void emulateSXTH(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = (uint32_t) signExtend(regs[Rm], 16);
}

static void emulateSXTB(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = (uint32_t) signExtend(regs[Rm], 8);
}

static void emulateUXTH(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = regs[Rm] & 0xFFFF;
}

static void emulateUXTB(uint16_t instr)
{
    unsigned Rm = (instr >> 3) & 0x7;
    unsigned Rdn = instr & 0x7;

    regs[Rdn] = regs[Rm] & 0xFF;
}

static void emulateMOV(uint16_t instr)
{
    // Thumb T5 encoding, does not affect flags.
    // This subset does not support high register access.

    unsigned Rs = (instr >> 3) & 0x7;
    unsigned Rd = instr & 0x7;

    regs[Rd] = regs[Rs];
}


///////////////////////////////////////////////
// B R A N C H I N G   I N S T R U C T I O N S
///////////////////////////////////////////////

static void emulateB(uint16_t instr)
{
    reg_t oldPC = regs[REG_PC];
    reg_t newPC = branchTargetB(instr, oldPC);

    if (newPC != oldPC) {
        regs[REG_PC] = newPC;
        svmCyclesElapsed += MCTiming::CPU_PIPELINE_RELOAD;
        calculateElapsedTicks();
    }
}


static void emulateCondB(uint16_t instr)
{
    reg_t oldPC = regs[REG_PC];
    reg_t newPC = branchTargetCondB(instr, oldPC, regs[REG_CPSR]);

    if (newPC != oldPC) {
        regs[REG_PC] = newPC;
        svmCyclesElapsed += MCTiming::CPU_PIPELINE_RELOAD;
        calculateElapsedTicks();
    }
}

static void emulateCBZ_CBNZ(uint16_t instr)
{
    unsigned Rn = instr & 0x7;
    reg_t oldPC = regs[REG_PC];
    reg_t newPC = branchTargetCBZ_CBNZ(instr, oldPC, regs[REG_CPSR], regs[Rn]);

    if (newPC != oldPC) {
        regs[REG_PC] = newPC;
        svmCyclesElapsed += MCTiming::CPU_PIPELINE_RELOAD;
        calculateElapsedTicks();
    }
}


/////////////////////////////////////////
// M E M O R Y  I N S T R U C T I O N S
/////////////////////////////////////////

static void emulateSTRSPImm(uint16_t instr)
{
    // encoding T2 only
    unsigned Rt = (instr >> 8) & 0x7;
    unsigned imm8 = instr & 0xff;
    reg_t addr = regs[REG_SP] + (imm8 << 2);

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_STORE_ADDRESS);
    if (!SvmMemory::isAddrAligned(addr, 4))
        return emulateFault(F_STORE_ALIGNMENT);

    SvmMemory::squashPhysicalAddr(regs[Rt]);
    *reinterpret_cast<uint32_t*>(addr) = regs[Rt];

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateLDRSPImm(uint16_t instr)
{
    // encoding T2 only
    unsigned Rt = (instr >> 8) & 0x7;
    unsigned imm8 = instr & 0xff;
    reg_t addr = regs[REG_SP] + (imm8 << 2);

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_LOAD_ADDRESS);
    if (!SvmMemory::isAddrAligned(addr, 4))
        return emulateFault(F_LOAD_ALIGNMENT);

    regs[Rt] = *reinterpret_cast<uint32_t*>(addr);

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateADDSpImm(uint16_t instr)
{
    // encoding T1 only
    unsigned Rd = (instr >> 8) & 0x7;
    unsigned imm8 = instr & 0xff;

    /*
     * NB: We need to squash SP here on 64-bit systems, in order to keep
     *     usermode stack pointers consistent. On real hardware, addresses
     *     taken from the stack will always be 32-bit physical addresses,
     *     whereas addresses formed in other ways will be 32-bit virtual
     *     addresses. This is fine, since either form is valid, and there's
     *     no guarantee that pointer math between stack and heap addresses
     *     will produce a meaningful result.
     *
     *     However! On 64-bit, with our pointer-squashing, we need to worry
     *     about the difference between real 64-bit physical addresses, and
     *     addresses that have been squashed back to 32-bit. Normally stack
     *     addresses stay physical as they're copied from SP to r0-7, but that
     *     means that stack addresses which have been kept only in registers
     *     will differ from stack addresses that have ever made a trip into
     *     (32-bit) guest memory and have been squashed.
     *
     *     So, to keep all stack addresses consistent, we choose to use 32-bit
     *     virtual addresses everywhere when we're on 64-bit hosts. On 32-bit
     *     hosts, the squash will have no effect and we'll still be using
     *     32-bit physical addresses.
     */
    reg_t sp = regs[REG_SP];
    SvmMemory::squashPhysicalAddr(sp);
    
    regs[Rd] = sp + (imm8 << 2);
}

static void emulateLDRLitPool(uint16_t instr)
{
    unsigned Rt = (instr >> 8) & 0x7;
    unsigned imm8 = instr & 0xFF;

    // Round up to the next 32-bit boundary
    reg_t addr = ((regs[REG_PC] + 3) & ~3) + (imm8 << 2);

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_LOAD_ADDRESS);
    if (!SvmMemory::isAddrAligned(addr, 4))
        return emulateFault(F_LOAD_ALIGNMENT);

    regs[Rt] = *reinterpret_cast<uint32_t*>(addr);

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}


////////////////////////////////////////
// 3 2 - B I T  I N S T R U C T I O N S
////////////////////////////////////////

static void emulateSTR(uint32_t instr)
{
    unsigned imm12 = instr & 0xFFF;
    unsigned Rn = (instr >> 16) & 0xF;
    unsigned Rt = (instr >> 12) & 0xF;
    reg_t addr = regs[Rn] + imm12;

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_STORE_ADDRESS);
    if (!SvmMemory::isAddrAligned(addr, 4))
        return emulateFault(F_STORE_ALIGNMENT);

    SvmMemory::squashPhysicalAddr(regs[Rt]);
    *reinterpret_cast<uint32_t*>(addr) = regs[Rt];

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateLDR(uint32_t instr)
{
    unsigned imm12 = instr & 0xFFF;
    unsigned Rn = (instr >> 16) & 0xF;
    unsigned Rt = (instr >> 12) & 0xF;
    reg_t addr = regs[Rn] + imm12;

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_LOAD_ADDRESS);
    if (!SvmMemory::isAddrAligned(addr, 4))
        return emulateFault(F_LOAD_ALIGNMENT);

    regs[Rt] = *reinterpret_cast<uint32_t*>(addr);

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateSTRBH(uint32_t instr)
{
    const unsigned HalfwordBit = 1 << 21;

    unsigned imm12 = instr & 0xFFF;
    unsigned Rn = (instr >> 16) & 0xF;
    unsigned Rt = (instr >> 12) & 0xF;
    reg_t addr = regs[Rn] + imm12;

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_STORE_ADDRESS);

    if (instr & HalfwordBit) {
        if (!SvmMemory::isAddrAligned(addr, 2))
            return emulateFault(F_STORE_ALIGNMENT);
        *reinterpret_cast<uint16_t*>(addr) = regs[Rt];
    } else {
        *reinterpret_cast<uint8_t*>(addr) = regs[Rt];
    }

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateLDRBH(uint32_t instr)
{
    const unsigned HalfwordBit = 1 << 21;
    const unsigned SignExtBit = 1 << 24;

    unsigned imm12 = instr & 0xFFF;
    unsigned Rn = (instr >> 16) & 0xF;
    unsigned Rt = (instr >> 12) & 0xF;
    reg_t addr = regs[Rn] + imm12;

    if (!SvmMemory::isAddrValid(addr))
        return emulateFault(F_LOAD_ADDRESS);

    switch (instr & (HalfwordBit | SignExtBit)) {
    case 0:
        regs[Rt] = *reinterpret_cast<uint8_t*>(addr);
        break;
    case HalfwordBit:
        if (!SvmMemory::isAddrAligned(addr, 2))
            return emulateFault(F_LOAD_ALIGNMENT);
        regs[Rt] = *reinterpret_cast<uint16_t*>(addr);
        break;
    case SignExtBit:
        regs[Rt] = (uint32_t)signExtend(*reinterpret_cast<uint8_t*>(addr), 8);
        break;
    case HalfwordBit | SignExtBit:
        if (!SvmMemory::isAddrAligned(addr, 2))
            return emulateFault(F_LOAD_ALIGNMENT);
        regs[Rt] = (uint32_t)signExtend(*reinterpret_cast<uint16_t*>(addr), 16);
        break;
    }

    svmCyclesElapsed += MCTiming::CPU_LOAD_STORE;
}

static void emulateMOVWT(uint32_t instr)
{
    const unsigned TopBit = 1 << 23;

    unsigned Rd = (instr >> 8) & 0xF;
    unsigned imm16 =
        (instr & 0x000000FF) |
        (instr & 0x00007000) >> 4 |
        (instr & 0x04000000) >> 15 |
        (instr & 0x000F0000) >> 4;

    if (TopBit & instr) {
        regs[Rd] = (regs[Rd] & 0xFFFF) | (imm16 << 16);
    } else {
        regs[Rd] = imm16;
    }
}

static void emulateDIV(uint32_t instr)
{
    const unsigned UnsignedBit = 1 << 21;

    unsigned Rn = (instr >> 16) & 0xF;
    unsigned Rd = (instr >> 8) & 0xF;
    unsigned Rm = instr & 0xF;

    uint32_t m32 = (uint32_t) regs[Rm];

    if (m32 == 0) {
        // Divide by zero, defined to return 0
        regs[Rd] = 0;
    } else if (UnsignedBit & instr) {
        regs[Rd] = (uint32_t)regs[Rn] / m32;
    } else {
        regs[Rd] = (int32_t)regs[Rn] / (int32_t)m32;
    }

    svmCyclesElapsed += MCTiming::CPU_DIVIDE;
}

static void emulateCLZ(uint32_t instr)
{
    unsigned Rm1 = (instr >> 16) & 0xF;
    unsigned Rd  = (instr >> 8) & 0xF;
    unsigned Rm2 = instr & 0xF;

    // ARM ARM states that the two Rm fields must be consistent
    if (Rm1 != Rm2)
        return emulateFault(F_CPU_SIM);
    uint32_t m32 = (uint32_t) regs[Rm1];

    // Note that GCC leaves __builtin_clz(0) undefined, whereas for ARM it's 32.
    if (m32 == 0)
        regs[Rd] = 32;
    else
        regs[Rd] = __builtin_clz(m32);
}


/***************************************************************************
 * Instruction Dispatch
 ***************************************************************************/

static void traceFetch(uint16_t *pc)
{
    unsigned va = SvmRuntime::reconstructCodeAddr(regs[REG_PC]);
    unsigned fa = SvmMemory::virtToFlashAddr(va);

    LOG(("[va=%08x fa=%08x pa=%p i=%04x]", va, fa, pc, *pc));

    for (unsigned r = 0; r < 8; r++) {
        // Display as a fixed-width low word and a variable-width high word.
        // The high word will usually be zero, and it helps to demarcate the
        // word boundary. On 32-bit hosts, the top word is *always* zero.
        uint64_t val = regs[r];
        LOG((" r%d=%x:%08x", r, (unsigned)(val >> 32), (unsigned)val));
    }
    LOG((" (%c%c%c%c) | r8=%p r9=%p sp=%p fp=%p\n",
        getNeg() ? 'N' : ' ',
        getZero() ? 'Z' : ' ',
        getCarry() ? 'C' : ' ',
        getOverflow() ? 'V' : ' ',
        reinterpret_cast<void*>(regs[8]),
        reinterpret_cast<void*>(regs[9]),
        reinterpret_cast<void*>(regs[REG_SP]),
        reinterpret_cast<void*>(regs[REG_FP])));
}

static uint16_t fetch()
{
    /*
     * Fetch the next instruction.
     * We can return 16bit values unconditionally, since all our instructions are Thumb,
     * and the first nibble of a 32-bit instruction must be checked regardless
     * in order to determine its bitness.
     */

    svmCyclesElapsed += MCTiming::CPU_FETCH;

    if (!SvmMemory::isAddrValid(regs[REG_PC])) {
        emulateFault(F_CODE_FETCH);
        return Nop;
    }
    if (!SvmMemory::isAddrAligned(regs[REG_PC], 2)) {
        emulateFault(F_LOAD_ALIGNMENT);
        return Nop;
    }

    /*
     * As we execute, check that each address we hit is
     * part of a valid bundle according to SvmValidator. This
     * serves to double-check both the validator and this runtime.
     */
    DEBUG_ONLY({
        SvmMemory::VirtAddr bundleVA = SvmRuntime::reconstructCodeAddr(regs[REG_PC]);
        SvmMemory::PhysAddr pa;
        FlashBlockRef ref;
        bundleVA &= ~(Svm::BUNDLE_SIZE - 1);
        ASSERT(SvmMemory::mapROCode(ref, bundleVA, pa));
    });

    uint16_t *pc = reinterpret_cast<uint16_t*>(regs[REG_PC]);
    TRACING_ONLY(traceFetch(pc));

    regs[REG_PC] += sizeof(uint16_t);
    return *pc;
}

static void execute16(uint16_t instr)
{
    if ((instr & AluMask) == AluTest) {
        // lsl, lsr, asr, add, sub, mov, cmp
        // take bits [13:11] to group these
        uint8_t prefix = (instr >> 11) & 0x7;
        switch (prefix) {
        case 0: // 0b000 - LSL
            emulateLSLImm(instr);
            return;
        case 1: // 0b001 - LSR
            emulateLSRImm(instr);
            return;
        case 2: // 0b010 - ASR
            emulateASRImm(instr);
            return;
        case 3: { // 0b011 - ADD/SUB reg/imm
            uint8_t subop = (instr >> 9) & 0x3;
            switch (subop) {
            case 0:
                emulateADDReg(instr);
                return;
            case 1:
                emulateSUBReg(instr);
                return;
            case 2:
                emulateADD3Imm(instr);
                return;
            case 3:
                emulateADD8Imm(instr);
                return;
            }
        }
        case 4: // 0b100 - MOV
            emulateMovImm(instr);
            return;
        case 5: // 0b101
            emulateCmpImm(instr);
            return;
        case 6: // 0b110 - ADD 8bit
            emulateADD8Imm(instr);
            return;
        case 7: // 0b111 - SUB 8bit
            emulateSUB8Imm(instr);
            return;
        }
        ASSERT(0 && "unhandled ALU instruction!");
    }
    if ((instr & DataProcMask) == DataProcTest) {
        uint8_t opcode = (instr >> 6) & 0xf;
        switch (opcode) {
        case 0:  emulateANDReg(instr); return;
        case 1:  emulateEORReg(instr); return;
        case 2:  emulateLSLReg(instr); return;
        case 3:  emulateLSRReg(instr); return;
        case 4:  emulateASRReg(instr); return;
        case 5:  emulateADCReg(instr); return;
        case 6:  emulateSBCReg(instr); return;
        case 7:  emulateRORReg(instr); return;
        case 8:  emulateTSTReg(instr); return;
        case 9:  emulateRSBImm(instr); return;
        case 10: emulateCMPReg(instr); return;
        case 11: emulateCMNReg(instr); return;
        case 12: emulateORRReg(instr); return;
        case 13: emulateMUL(instr);    return;
        case 14: emulateBICReg(instr); return;
        case 15: emulateMVNReg(instr); return;
        }
    }
    if ((instr & MiscMask) == MiscTest) {
        uint8_t opcode = (instr >> 5) & 0x7f;
        if ((opcode & 0x78) == 0x2) {   // bits [6:3] of opcode identify this group
            switch (opcode & 0x6) {     // bits [2:1] of the opcode identify the instr
            case 0: emulateSXTH(instr); return;
            case 1: emulateSXTB(instr); return;
            case 2: emulateUXTH(instr); return;
            case 3: emulateUXTB(instr); return;
            }
        }
    }
    if ((instr & MovMask) == MovTest) {
        emulateMOV(instr);
        return;
    }    
    if ((instr & SvcMask) == SvcTest) {
        emulateSVC(instr);
        return;
    }
    if ((instr & PcRelLdrMask) == PcRelLdrTest) {
        emulateLDRLitPool(instr);
        return;
    }
    if ((instr & SpRelLdrStrMask) == SpRelLdrStrTest) {
        uint16_t isLoad = instr & (1 << 11);
        if (isLoad)
            emulateLDRSPImm(instr);
        else
            emulateSTRSPImm(instr);
        return;
    }
    if ((instr & SpRelAddMask) == SpRelAddTest) {
        emulateADDSpImm(instr);
        return;
    }
    if ((instr & UncondBranchMask) == UncondBranchTest) {
        emulateB(instr);
        return;
    }
    if ((instr & CompareBranchMask) == CompareBranchTest) {
        emulateCBZ_CBNZ(instr);
        return;
    }
    if ((instr & CondBranchMask) == CondBranchTest) {
        emulateCondB(instr);
        return;
    }
    if (instr == Nop) {
        // nothing to do
        return;
    }

    // should never get here since we should only be executing validated instructions
    LOG(("SVMCPU: invalid 16bit instruction: 0x%x\n", instr));
    return emulateFault(F_CPU_SIM);
}

static void execute32(uint32_t instr)
{
    if ((instr & StrMask) == StrTest) {
        emulateSTR(instr);
        return;
    }
    if ((instr & StrBhMask) == StrBhTest) {
        emulateSTRBH(instr);
        return;
    }
    if ((instr & LdrBhMask) == LdrBhTest) {
        emulateLDRBH(instr);
        return;
    }
    if ((instr & LdrMask) == LdrTest) {
        emulateLDR(instr);
        return;
    }
    if ((instr & MovWtMask) == MovWtTest) {
        emulateMOVWT(instr);
        return;
    }
    if ((instr & DivMask) == DivTest) {
        emulateDIV(instr);
        return;
    }
    if ((instr & ClzMask) == ClzTest) {
        emulateCLZ(instr);
        return;
    }

    // should never get here since we should only be executing validated instructions
    LOG(("SVMCPU: invalid 32bit instruction: 0x%x\n", instr));
    return emulateFault(F_CPU_SIM);
}


/***************************************************************************
 * Public Functions
 ***************************************************************************/

void run(reg_t sp, reg_t pc)
{
    regs[REG_SP] = sp;
    regs[REG_PC] = pc;

    for (;;) {
        uint16_t instr = fetch();
        if (instructionSize(instr) == InstrBits16) {
            execute16(instr);
        }
        else {
            uint16_t instrLow = fetch();
            execute32(instr << 16 | instrLow);
        }
    }
}


}  // namespace SvmCpu
