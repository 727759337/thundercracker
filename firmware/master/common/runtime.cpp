/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "runtime.h"
#include "cube.h"
#include "neighbors.h"
#ifndef SIFTEO_SIMULATOR
    #include "tasks.h"
#endif

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/Support/IRReader.h"

using namespace llvm;
using namespace Sifteo;

jmp_buf Runtime::jmpExit;

bool Event::dispatchInProgress;
uint32_t Event::pending;
uint32_t Event::eventCubes[EventBits::COUNT];
bool Event::paused = false;


void Runtime::run()
{
    LLVMContext &Context = getGlobalContext();

    SMDiagnostic Err;
    Module *Mod = ParseIRFile("runtime.bc", Err, Context);
    ASSERT(Mod != NULL && "ERROR: Runtime failed to open bitcode, make sure runtime.bc is in the current directory");
        
    EngineBuilder builder(Mod);
    builder.setEngineKind(EngineKind::Interpreter);

    ExecutionEngine *EE = builder.create();
    ASSERT(EE != NULL);

    std::vector<GenericValue> args;
    Function *EntryFn = Mod->getFunction("siftmain");
    ASSERT(EntryFn != NULL);

    if (setjmp(jmpExit))
        return;

    EE->runStaticConstructorsDestructors(false);
    EE->runFunction(EntryFn, args);
    EE->runStaticConstructorsDestructors(true);    
}

void Runtime::exit()
{
    longjmp(jmpExit, 1);
}

void Event::dispatch()
{
    /*
     * Skip event dispatch if we're already in an event handler
     */

    if (dispatchInProgress || paused)
        return;
    dispatchInProgress = true;

    /*
     * Process events, by type
     */

    while (pending) {
        EventBits::ID event = (EventBits::ID)Intrinsic::CLZ(pending);

		while (eventCubes[event]) {
                _SYSCubeID slot = Intrinsic::CLZ(eventCubes[event]);
                if (event <= EventBits::LAST_CUBE_EVENT) {
                    callCubeEvent(event, slot);
                } else if (event == EventBits::NEIGHBOR) {
                    NeighborSlot::instances[slot].computeEvents();
                }
                
                Atomic::And(eventCubes[event], ~Intrinsic::LZ(slot));
            }
        Atomic::And(pending, ~Intrinsic::LZ(event));
    }

    dispatchInProgress = false;
}

/*
 * XXX: Temporary syscall trampolines for LLVM interpreter.
 * XXX: Refactor syscalls to reduce number of different signatures?
 */

#define LLVM_SYS_VOID(name) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 0); \
         _SYS_##name(); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_PTR(name, t) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 1); \
         _SYS_##name((t*)GVTOP(args[0])); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_I1_PTR(name, t) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 2); \
         _SYS_##name(args[0].IntVal.getZExtValue(), (t*)GVTOP(args[1])); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_I1(name) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 1); \
         _SYS_##name(args[0].IntVal.getZExtValue()); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_I2(name) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 2); \
         _SYS_##name(args[0].IntVal.getZExtValue(), args[1].IntVal.getZExtValue()); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_PTR_I1(name, t) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 2); \
         _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue()); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_PTR_I2(name, t) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 3); \
         _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), args[2].IntVal.getZExtValue()); \
         return GenericValue(); \
     }

#define LLVM_SYS_VOID_PTR_I3(name, t) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 4); \
         _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), args[2].IntVal.getZExtValue(), args[3].IntVal.getZExtValue()); \
         return GenericValue(); \
     }
     
#define LLVM_SYS_VOID_PTR_I_PTR(name, t, u) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 3); \
         _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), (u*)GVTOP(args[2])); \
         return GenericValue(); \
     }

#define LLVM_SYS_I_PTR_PTR_ENUM(name, t, u, v) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 3); \
         GenericValue ret; \
         ret.IntVal = _SYS_##name((t*)GVTOP(args[0]), (u*)GVTOP(args[1]), (v)args[2].IntVal.getZExtValue()); \
         return ret; \
     }

#define LLVM_SYS_I_I1(name) \
     GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
     { \
         ASSERT(args.size() == 1); \
         GenericValue ret; \
         ret.IntVal = _SYS_##name(args[0].IntVal.getZExtValue()); \
         return ret; \
     }
     
#define LLVM_SYS_VOID_PTR_I_PTR_I1(name, t, u) \
          GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
          { \
              ASSERT(args.size() == 4); \
              _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), (u*)GVTOP(args[2]), \
                          args[3].IntVal.getZExtValue()); \
              return GenericValue(); \
          }

#define LLVM_SYS_VOID_PTR_I_PTR_I2(name, t, u) \
          GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
          { \
              ASSERT(args.size() == 5); \
              _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), (u*)GVTOP(args[2]), \
                          args[3].IntVal.getZExtValue(), args[4].IntVal.getZExtValue()); \
              return GenericValue(); \
          }

#define LLVM_SYS_VOID_PTR_I_PTR_I5(name, t, u) \
          GenericValue lle_X__SYS_##name(FunctionType *ft, const std::vector<GenericValue> &args) \
          { \
              ASSERT(args.size() == 8); \
              _SYS_##name((t*)GVTOP(args[0]), args[1].IntVal.getZExtValue(), (u*)GVTOP(args[2]), \
                          args[3].IntVal.getZExtValue(), args[4].IntVal.getZExtValue(), \
                          args[5].IntVal.getZExtValue(), args[6].IntVal.getZExtValue(), \
                          args[7].IntVal.getZExtValue()); \
              return GenericValue(); \
          }

extern "C" {

    LLVM_SYS_VOID(exit)
    LLVM_SYS_VOID(yield)
    LLVM_SYS_VOID(paint)
    LLVM_SYS_VOID(finish)
    LLVM_SYS_VOID_PTR(ticks_ns, int64_t)
    LLVM_SYS_VOID_PTR(vbuf_init, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR(vbuf_unlock, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR(audio_enableChannel, _SYSAudioBuffer)
    LLVM_SYS_VOID_I1(enableCubes)
    LLVM_SYS_VOID_I1(disableCubes)
    LLVM_SYS_VOID_I1(audio_stop)
    LLVM_SYS_VOID_I1(audio_pause)
    LLVM_SYS_VOID_I1(audio_resume)
    LLVM_SYS_VOID_I1_PTR(setVideoBuffer, _SYSVideoBuffer)
    LLVM_SYS_VOID_I1_PTR(loadAssets, _SYSAssetGroup)
    LLVM_SYS_VOID_I1_PTR(getAccel, _SYSAccelState)
    LLVM_SYS_VOID_I1_PTR(getNeighbors, _SYSNeighborState)
    LLVM_SYS_VOID_I1_PTR(getTilt, _SYSTiltState)
    LLVM_SYS_VOID_I1_PTR(getShake, _SYSShakeState)
    LLVM_SYS_VOID_I1_PTR(getRawNeighbors, uint8_t)
    LLVM_SYS_VOID_I1_PTR(getRawBatteryV, uint16_t)
    LLVM_SYS_VOID_I1_PTR(getCubeHWID, _SYSCubeHWID)
    LLVM_SYS_VOID_I2(solicitCubes)
    LLVM_SYS_VOID_I2(audio_setVolume)
    LLVM_SYS_VOID_PTR_I1(vbuf_lock, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I2(vbuf_poke, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I2(vbuf_pokeb, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I3(vbuf_spr_resize, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I3(vbuf_spr_move, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I3(vbuf_fill, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I3(vbuf_seqi, _SYSVideoBuffer)
    LLVM_SYS_VOID_PTR_I_PTR(vbuf_peek, _SYSVideoBuffer, uint16_t)
    LLVM_SYS_VOID_PTR_I_PTR(vbuf_peekb, _SYSVideoBuffer, uint8_t)
    LLVM_SYS_I_I1(audio_isPlaying)
    LLVM_SYS_I_I1(audio_volume)
    LLVM_SYS_I_I1(audio_pos)
    LLVM_SYS_I_PTR_PTR_ENUM(audio_play, _SYSAudioModule, _SYSAudioHandle, _SYSAudioLoopType)
    LLVM_SYS_VOID_PTR_I_PTR_I1(vbuf_write, _SYSVideoBuffer, uint16_t)
    LLVM_SYS_VOID_PTR_I_PTR_I2(vbuf_writei, _SYSVideoBuffer, uint16_t)
    LLVM_SYS_VOID_PTR_I_PTR_I5(vbuf_wrect, _SYSVideoBuffer, uint16_t)
}
 
/*
 * XXX: Temporary trampolines for libc functions that have leaked into games.
 *      These should be redesigned in terms of syscalls and our ABI!
 */
 
extern "C" {

    GenericValue lle_X_memset(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        ASSERT(args.size() == 3);
        memset(GVTOP(args[0]), args[1].IntVal.getZExtValue(), args[2].IntVal.getZExtValue());
        return GenericValue();
    }

    GenericValue lle_X_memcpy(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        ASSERT(args.size() == 3);
        memcpy(GVTOP(args[0]), GVTOP(args[1]), args[2].IntVal.getZExtValue());
        return GenericValue();
    }

    GenericValue lle_X_rand_r(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        ASSERT(args.size() == 1);
        GenericValue ret;
        ret.IntVal = rand_r((unsigned *)GVTOP(args[0]));
        return ret;
    }

    GenericValue lle_X_sinf(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        ASSERT(args.size() == 1);
        GenericValue ret;
        ret.FloatVal = sinf(args[0].FloatVal);
        return ret;
    }

    GenericValue lle_X_cosf(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        ASSERT(args.size() == 1);
        GenericValue ret;
        ret.FloatVal = cosf(args[0].FloatVal);
        return ret;
    }

    GenericValue lle_X_vsnprintf(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        // XXX: Total stub!
        return GenericValue();
    }

    GenericValue lle_X_snprintf(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        // XXX: Total stub!
        return GenericValue();
    }

    GenericValue lle_X_strlen(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        // XXX: Total stub!
        return GenericValue();
    }

    GenericValue lle_X_strcpy(FunctionType *ft, const std::vector<GenericValue> &args)
    {
        // XXX: Total stub!
        return GenericValue();
    }
}