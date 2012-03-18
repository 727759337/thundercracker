/*
 * This file is part of the Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/ADT/SmallVector.h"
#include <utility>
#include <map>
using namespace llvm;

namespace llvm {

class CallSite;
class Instruction;

void initializeUUIDGeneratorPass(PassRegistry&);

class UUIDGenerator : public ModulePass {
public:
    static char ID;
    UUIDGenerator() : ModulePass(ID) {
        initializeUUIDGeneratorPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);

    virtual const char *getPassName() const {
        return "UUID generator pass";
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();
    }

    uint32_t getValueFor(CallSite &CS) const;

private:
    struct UUID_t {
        union {
            struct {
                uint32_t time_low;
                uint16_t time_mid;
                uint16_t time_hi_and_version;
                uint8_t clk_seq_hi_res;
                uint8_t clk_seq_low;
                uint8_t node[6];
            };
            uint8_t  bytes[16];
            uint16_t hwords[8];
            uint32_t words[4];
        };
    };

    struct Args {
        unsigned key;
        unsigned index;
    };

    typedef std::map<unsigned, UUID_t> ValueMap_t;
    ValueMap_t ValueMap;

    static void argUnpack(CallSite &CS, Args &a);

    void runOnFunction(Function &F);
    void runOnBasicBlock(BasicBlock &BB);
    void runOnCall(CallSite &CS);

    static UUID_t generate();
};

}  // end namespace llvm
