/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

extern "C" {
    #ifdef WIN32
    #   include <Winsock2.h>
    #   include <Rpc.h>
    #else
    #   include <uuid/uuid.h>
    #endif
}

#include "UUIDGenerator.h"
#include "Support/ErrorReporter.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Support/CallSite.h"
using namespace llvm;

char UUIDGenerator::ID = 0;
INITIALIZE_PASS(UUIDGenerator, "uuid-generator",
                "UUID generator pass", false, true)


bool UUIDGenerator::runOnModule(Module &M)
{
    for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I)
        runOnFunction(*I);
    return false;
}

void UUIDGenerator::runOnFunction(Function &F)
{
    for (Function::iterator I = F.begin(), E = F.end(); I != E; ++I)
        runOnBasicBlock(*I);
}

void UUIDGenerator::runOnBasicBlock (BasicBlock &BB)
{
    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E;) {
        CallSite CS(cast<Value>(I));
        ++I;

        if (CS) {
            Function *F = CS.getCalledFunction();
            if (F && F->getName() == "_SYS_lti_uuid")
                runOnCall(CS);
        }
    }
}

void UUIDGenerator::argUnpack(CallSite &CS, Args &a)
{
    Instruction *I = CS.getInstruction();

    if (CS.arg_size() != 2)
        report_fatal_error(I, "_SYS_lti_uuid requires two arguments");

    ConstantInt *CI = dyn_cast<ConstantInt>(CS.getArgument(0));
    if (!CI)
        report_fatal_error(I, "_SYS_lti_uuid key must be a constant integer");
    a.key = CI->getZExtValue();

    CI = dyn_cast<ConstantInt>(CS.getArgument(1));
    if (!CI)
        report_fatal_error(I, "_SYS_lti_uuid index must be a constant integer");
    a.index = CI->getZExtValue();
    if (a.index >= 4)
        report_fatal_error(I, "_SYS_lti_uuid index out of range");
}

void UUIDGenerator::runOnCall(CallSite &CS)
{
    Args a;
    argUnpack(CS, a);
    if (ValueMap.count(a.key) == 0)
        ValueMap[a.key] = generate();
}

uint32_t UUIDGenerator::getValueFor(CallSite &CS) const
{
    Args a;
    argUnpack(CS, a);
    ValueMap_t::const_iterator I = ValueMap.find(a.key);
    
    assert(I != ValueMap.end() && "Call site was never seen by UUIDGenerator");
    assert(a.index < 4);

    return I->second.words[a.index];
}

UUIDGenerator::UUID_t UUIDGenerator::generate()
{
    UUID_t result;

    #ifdef WIN32

        // Win32 UUIDs, use a mixture of little-endian and an octet stream
        UUID uuid;
        UuidCreate(&uuid);
        result.time_low = htonl(uuid.Data1);
        result.time_mid = htons(uuid.Data2);
        result.time_hi_and_version = htons(uuid.Data3);
        memcpy(&result.clk_seq_hi_res, uuid.Data4, sizeof uuid.Data4);

    #else // !WIN32

        // OSF DCE standard. Uses network byte order.
        uuid_t uuid;
        uuid_generate_random(uuid);
        assert(sizeof result == sizeof uuid);
        memcpy(result.bytes, uuid, sizeof result.bytes);

    #endif

    return result;
}
