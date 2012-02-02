/*
 * This file is part of the Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "SVM.h"
#include "SVMMCTargetDesc.h"
#include "SVMTargetMachine.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

extern "C" void LLVMInitializeSVMTarget()
{
    RegisterTargetMachine<SVMTargetMachine> X(TheSVMTarget);
}

SVMTargetMachine::SVMTargetMachine(const Target &T, StringRef TT, 
                                   StringRef CPU, StringRef FS,
                                   Reloc::Model RM, CodeModel::Model CM)
    : LLVMTargetMachine(T, TT, CPU, FS, RM, CM),
      DataLayout("e-S32-p:32:32:32-i:32:32:32-f32:32:32-n32"),
      TLInfo(*this), TSInfo(*this), Subtarget(TT, CPU, FS)
{}

bool SVMTargetMachine::addInstSelector(PassManagerBase &PM, CodeGenOpt::Level OptLevel)
{
    PM.add(createSVMISelDag(*this));
    return false;
}

bool SVMTargetMachine::addPreEmitPass(PassManagerBase &PM, CodeGenOpt::Level OptLevel)
{
    PM.add(createSVMAlignPass(*this));
    return true;
}

uint32_t SVMTargetMachine::getBlockSize()
{
    return 256;
}

uint32_t SVMTargetMachine::getBundleSize()
{
    return 4;
}

uint32_t SVMTargetMachine::getFlashBase()
{
    return 0x80000000;
}

uint32_t SVMTargetMachine::getRAMBase()
{
    return 0x10000;
}

void SVMTargetMachine::buildSysCallMap(StringMap<uint16_t> &Map)
{
    Map["_SYS_paint"] = 0x0100;   
    Map["_SYS_ticks_ns"] = 0x0101;
}
