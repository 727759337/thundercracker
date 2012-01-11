/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */
 
/*
 * Trace logging support, for development use only.
 * Requires a firmware image. (Intentionally disabled with SBT)
 */

#ifndef _TRACER_H
#define _TRACER_H

#include <stdio.h>
#include <stdarg.h>
#include "macros.h"
#include "vcdwriter.h"
#include "cube_cpu.h"


class Tracer {
 public:
    Tracer()
        : epochIsSet(false), textTraceFile(NULL), vcdTraceFile(NULL) {}

    VCDWriter vcd;
     
    void setEnabled(bool b);     
    void close();

    ALWAYS_INLINE void tick(const VirtualTime &vtime) {
        if (isEnabled())
            vcd.writeTick(vcdTraceFile, getLocalClock(vtime));
    }

    ALWAYS_INLINE static bool isEnabled() {
        return UNLIKELY(enabled);
    }
    
    static void log(const Cube::CPU::em8051 *cpu, const char *fmt, ...)
        __attribute__ ((format(printf,2,3)))
    {
        if (isEnabled()) {
            va_list ap;
            va_start(ap, fmt);
            instance->logWork(cpu, fmt, ap);
            va_end(ap);
        }
    }

    static ALWAYS_INLINE void logHex(const Cube::CPU::em8051 *cpu, const char *msg, size_t len, void *data)
    {
        if (isEnabled())
            instance->logHexWork(cpu, msg, len, data);
    }
    
 private:
    static bool enabled;
    static Tracer *instance;
     
    bool epochIsSet;
    uint64_t epoch;

    FILE *textTraceFile;
    FILE *vcdTraceFile;
    
    uint64_t getLocalClock(const VirtualTime &vtime)
    {
        /*
         * Use our own local epoch, so that logged timestamps start at zero.
         */

        if (epochIsSet) {
            return vtime.clocks - epoch;
        } else {
            epoch = vtime.clocks;
            epochIsSet = true;
            return 0;
        }
    }
    
    void logWork(const Cube::CPU::em8051 *cpu);
    void logWork(const Cube::CPU::em8051 *cpu, const char *fmt, va_list ap);
    void logHexWork(const Cube::CPU::em8051 *cpu, const char *msg, size_t len, void *data);
};

#endif
