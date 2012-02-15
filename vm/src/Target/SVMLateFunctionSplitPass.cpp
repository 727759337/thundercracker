/*
 * This file is part of the Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * This is our last-resort for dealing with functions that are larger than
 * one flash block. We prefer to use higher-level (and target
 * independent) transforms to reduce the size of functions, but those
 * can't know how large a function will be once it's fully compiled.
 *
 * This pass operates on already-aligned MachineFunctions, which we can
 * precisely measure the size of. Since earlier passes were responsible for
 * splitting functions at natural places, here we just blindly stuff as much
 * code into a single block as possible.
 *
 * We must walk through the function linearly, keeping an accurate count
 * of how many bytes it's spent on instructions, constant pool data, and
 * how many additional bytes we'll need for reworking branches if a split
 * does become necessary.
 *
 * Just before we would have overflowed a flash block with any one of these
 * kinds of data, this pass adds a SPLIT pseudo-instruction. By adding the
 * SPLITs and converting near branches to long branches, it becomes trivial
 * for later passes to allocate these functions into multiple blocks.
 */

#include "SVM.h"
#include "SVMTargetMachine.h"
#include "SVMBlockSizeAccumulator.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Module.h"
#include "llvm/codeGen/MachineInstr.h"
#include "llvm/codeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include <vector>
using namespace llvm;

namespace {

    class SVMLateFunctionSplitPass : public MachineFunctionPass {
        SVMTargetMachine& TM;

    public:
        static char ID;
        explicit SVMLateFunctionSplitPass(SVMTargetMachine &tm)
            : MachineFunctionPass(ID), TM(tm) {}

        bool runOnMachineFunction(MachineFunction &MF);

        const char *getPassName() const {
            return "SVM late function splitter pass";
        }
    };
  
    char SVMLateFunctionSplitPass::ID = 0;
}

FunctionPass *llvm::createSVMLateFunctionSplitPass(SVMTargetMachine &TM)
{
    return new SVMLateFunctionSplitPass(TM);
}

bool SVMLateFunctionSplitPass::runOnMachineFunction(MachineFunction &MF)
{
    const TargetInstrInfo &TII = *TM.getInstrInfo();
    SVMBlockSizeAccumulator BSA;
    bool Changed = false;

    // Start measuring from the beginning of the function
    BSA.clear();

    // Iterate over basic blocks...
    for (MachineFunction::iterator MBB = MF.begin(); MBB != MF.end(); ++MBB) {

        // All basic blocks must align to a bundle boundary
        BSA.InstrAlign(TM.getBundleSize());

        // Iterate over instructions...
        for (MachineBasicBlock::iterator I = MBB->begin(); I != MBB->end(); ++I) {

            // Look for the first instruction that puts us over-budget
            BSA.AddInstr(I);
            if (BSA.getByteCount() <= TM.getBlockSize())
                continue;

            Changed = true;

            // The new block begins with instruction I
            BSA.clear();
            BSA.AddInstr(I);
    
            // Insert a split just prior to I
            BuildMI(*MBB, I, I->getDebugLoc(), TII.get(SVM::SPLIT));
        }
    }
    
    return Changed;
}
