/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

/*
 * Syscalls for miscellaneous operations which globally effect the runtime
 * execution environment.
 */

#include <sifteo/abi.h>
#include "svmmemory.h"
#include "svmdebugpipe.h"
#include "svmdebugger.h"
#include "svmruntime.h"
#include "svmloader.h"
#include "svmclock.h"
#include "radio.h"
#include "cubeslots.h"
#include "event.h"
#include "tasks.h"
#include "shutdown.h"
#include "idletimeout.h"
#include "ui_pause.h"
#include "ui_coordinator.h"
#include "ui_shutdown.h"

extern "C" {

uint32_t _SYS_getFeatures()
{
    // Reserved for future use. There are no feature bits yet.
    return 0;
}

void _SYS_abort()
{
    SvmRuntime::fault(Svm::F_ABORT);
}

void _SYS_exit(void)
{
    SvmDebugger::signal(Svm::Debugger::S_TERM);
    SvmLoader::exit();
}

void _SYS_yield(void)
{
    Tasks::idle();
    SvmRuntime::dispatchEventsOnReturn();
}

void _SYS_paint(void)
{
    CubeSlots::paintCubes(CubeSlots::userConnected);
    SvmRuntime::dispatchEventsOnReturn();
}

void _SYS_paintUnlimited(void)
{
    CubeSlots::paintCubes(CubeSlots::userConnected, false);
    SvmRuntime::dispatchEventsOnReturn();
}

void _SYS_finish(void)
{
    CubeSlots::finishCubes(CubeSlots::userConnected);
    // Intentionally does _not_ dispatch events!
}

int64_t _SYS_ticks_ns(void)
{
    return SvmClock::ticks();
}

void _SYS_setVector(_SYSVectorID vid, void *handler, void *context)
{
    if (vid < _SYS_NUM_VECTORS)
        return Event::setVector(vid, handler, context);

    SvmRuntime::fault(F_SYSCALL_PARAM);
}

void *_SYS_getVectorHandler(_SYSVectorID vid)
{
    if (vid < _SYS_NUM_VECTORS)
        return Event::getVectorHandler(vid);

    SvmRuntime::fault(F_SYSCALL_PARAM);
    return NULL;
}

void *_SYS_getVectorContext(_SYSVectorID vid)
{
    if (vid < _SYS_NUM_VECTORS)
        return Event::getVectorContext(vid);

    SvmRuntime::fault(F_SYSCALL_PARAM);
    return NULL;
}

void _SYS_setGameMenuLabel(const char *label)
{
    if (label) {
        if (!UIPause::setGameMenuLabel(reinterpret_cast<SvmMemory::VirtAddr>(label)))
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
    } else {
        UIPause::disableGameMenu();
    }
}

void _SYS_shutdown(uint32_t flags)
{
    const uint32_t excludedTasks =
        Intrinsic::LZ(Tasks::AudioPull)  |
        Intrinsic::LZ(Tasks::Pause);

    SvmClock::pause();

    if (flags & _SYS_SHUTDOWN_WITH_UI) {
        UICoordinator uic(excludedTasks);
        UIShutdown uiShutdown(uic);
        return uiShutdown.mainLoop();
    }

    ShutdownManager s(excludedTasks);
    return s.shutdown();
}

void _SYS_keepAwake()
{
    IdleTimeout::reset();
}

void _SYS_log(uint32_t t, uintptr_t v1, uintptr_t v2, uintptr_t v3,
    uintptr_t v4, uintptr_t v5, uintptr_t v6, uintptr_t v7)
{
    SvmLogTag tag(t);
    uint32_t type = tag.getType();
    uint32_t arity = tag.getArity();

    SvmMemory::squashPhysicalAddr(v1);
    SvmMemory::squashPhysicalAddr(v2);
    SvmMemory::squashPhysicalAddr(v3);
    SvmMemory::squashPhysicalAddr(v4);
    SvmMemory::squashPhysicalAddr(v5);
    SvmMemory::squashPhysicalAddr(v6);
    SvmMemory::squashPhysicalAddr(v7);

    switch (type) {

        // Stow all arguments, plus the log tag. The post-processor
        // will do some printf()-like formatting on the stored arguments.
        case _SYS_LOGTYPE_FMT: {
            uint32_t *buffer = SvmDebugPipe::logReserve(tag);        
            switch (arity) {
                case 7: buffer[6] = v7;
                case 6: buffer[5] = v6;
                case 5: buffer[4] = v5;
                case 4: buffer[3] = v4;
                case 3: buffer[2] = v3;
                case 2: buffer[1] = v2;
                case 1: buffer[0] = v1;
                case 0:
                default: break;
            }
            SvmDebugPipe::logCommit(tag, buffer, arity * sizeof(uint32_t));
            return;
        }

        // String messages: Only v1 is used (as a pointer), and we emit
        // zero or more log records until we hit the NUL terminator.
        case _SYS_LOGTYPE_STRING: {
            const unsigned chunkSize = SvmDebugPipe::LOG_BUFFER_BYTES;
            FlashBlockRef ref;
            for (;;) {
                uint32_t *buffer = SvmDebugPipe::logReserve(tag);
                if (!SvmMemory::copyROData(ref, (SvmMemory::PhysAddr)buffer,
                                    (SvmMemory::VirtAddr)v1, chunkSize)) {
                    SvmRuntime::fault(F_LOG_FETCH);
                    return;
                }

                // strnlen is not easily available on mingw...
                const char *str = reinterpret_cast<const char*>(buffer);
                const char *end = static_cast<const char*>(memchr(str, '\0', chunkSize));
                uint32_t bytes = end ? (size_t) (end - str) : chunkSize;

                SvmDebugPipe::logCommit(SvmLogTag(tag, bytes), buffer, bytes);
                if (bytes < chunkSize)
                    return;
                v1 += bytes;
            }
        }

        // Hex dumps: Like strings, v1 is used as a pointer. The inline
        // parameter from 'tag' is the length of the dump, in bytes. If we're
        // dumping more than what fits in a single log record, emit multiple
        // log records.
        case _SYS_LOGTYPE_HEXDUMP: {
            FlashBlockRef ref;
            uint32_t remaining = tag.getParam();
            while (remaining) {
                uint32_t chunkSize = MIN(SvmDebugPipe::LOG_BUFFER_BYTES, remaining);
                uint32_t *buffer = SvmDebugPipe::logReserve(tag);
                if (!SvmMemory::copyROData(ref, (SvmMemory::PhysAddr)buffer,
                                    (SvmMemory::VirtAddr)v1, chunkSize)) {
                    SvmRuntime::fault(F_LOG_FETCH);
                    return;
                }
                SvmDebugPipe::logCommit(SvmLogTag(tag, chunkSize), buffer, chunkSize);
                remaining -= chunkSize;
                v1 += chunkSize;
            }
            return;
        }

        // Tags that take no parameters
        case _SYS_LOGTYPE_SCRIPT: {
            SvmDebugPipe::logCommit(tag, SvmDebugPipe::logReserve(tag), 0);
            return;
        }

        default:
            ASSERT(0 && "Unknown _SYS_log() tag type");
            return;
    }
}

}  // extern "C"
