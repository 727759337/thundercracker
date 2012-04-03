/*
 * This file is part of the Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Our platform doesn't support nor need stack alignments larger than 32 bits.
 * This pass strips these unnecessary alignments from AllocaInst()s very early,
 * so that they don't affect optimization passes.
 */

#include "Support/ErrorReporter.h"
#include "Analysis/CounterAnalysis.h"
#include "Analysis/UUIDGenerator.h"
#include "Target/SVMSymbolDecoration.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

namespace llvm {
    BasicBlockPass *createMisalignStackPass();
}

extern cl::opt<bool> ELFDebug;

namespace {
    class MisalignStackPass : public BasicBlockPass {
    public:
        static char ID;
        MisalignStackPass()
            : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock (BasicBlock &BB);

        virtual const char *getPassName() const {
            return "Stack misalignment pass";
        }
    };
}

char MisalignStackPass::ID = 0;

BasicBlockPass *llvm::createMisalignStackPass()
{
    return new MisalignStackPass();
}

bool MisalignStackPass::runOnBasicBlock (BasicBlock &BB)
{
    bool Changed = false;
    const unsigned alignLimit = sizeof(uint32_t);

    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
        AllocaInst *AI = dyn_cast<AllocaInst>(I);

        if (AI && AI->getAlignment() > alignLimit) {
            AI->setAlignment(alignLimit);
            Changed = true;
        }
    }

    return Changed;
}
