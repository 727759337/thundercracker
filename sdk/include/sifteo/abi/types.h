/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the public interface to the Sifteo SDK.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _SIFTEO_ABI_TYPES_H
#define _SIFTEO_ABI_TYPES_H

#ifdef NOT_USERSPACE
#   include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Standard integer types. This is a subset of what's in stdint.h,
 * but we define them ourselves since system headers are not available.
 *
 * If we're doing a non-userspace build, however, these are pulled in
 * from stdint.h above.
 */

#ifndef NOT_USERSPACE
    typedef signed char int8_t;
    typedef unsigned char uint8_t;
    typedef signed short int16_t;
    typedef unsigned short uint16_t;
    typedef signed int int32_t;
    typedef unsigned int uint32_t;
    typedef signed long long int64_t;
    typedef unsigned long long uint64_t;
    typedef signed long intptr_t;
    typedef unsigned long uintptr_t;
#endif

// We can use 'bool' even in pure C code.
#ifndef __cplusplus
    typedef uint8_t bool;
#endif

/*
 * Basic data types which are valid across the user/system boundary.
 *
 * Data directions are from the userspace's perspective. IN is
 * firmware -> game, OUT is game -> firmware.
 */

#define _SYS_NUM_CUBE_SLOTS     32
#define _SYS_CUBE_ID_INVALID    0xFF    /// Reserved _SYSCubeID value

typedef uint8_t _SYSCubeID;             /// Cube slot index
typedef int8_t _SYSSideID;              /// Cube side index
typedef uint32_t _SYSCubeIDVector;      /// One bit for each cube slot, MSB-first
typedef uint8_t _SYSAssetSlot;          /// Ordinal for one of the game's asset slots

/*
 * Small vector types
 */

struct _SYSInt2 {
    int32_t x, y;
};

union _SYSByte4 {
    uint32_t value;
    struct {
        int8_t x, y, z, w;
    };
};

union _SYSNeighborState {
    uint32_t value;
    _SYSCubeID sides[4];
};

/*
 * Type bits, for use in the 'tag' for the low-level _SYS_log() handler.
 * Normally these don't need to be used in usermode code, they're inserted
 * automatically by slinky when expanding _SYS_lti_log().
 */

#define _SYS_LOGTYPE_FMT            0       // param = strtab offest
#define _SYS_LOGTYPE_STRING         1       // param = 0, v1 = ptr
#define _SYS_LOGTYPE_HEXDUMP        2       // param = length, v1 = ptr
#define _SYS_LOGTYPE_SCRIPT         3       // param = script type

#define _SYS_SCRIPT_NONE            0       // Normal logging
#define _SYS_SCRIPT_LUA             1       // Built-in Lua interpreter

/*
 * Internal state of the Pseudorandom Number Generator, maintained in user RAM.
 */

struct _SYSPseudoRandomState {
    uint32_t a, b, c, d;
};

/*
 * Hardware IDs are 64-bit numbers that uniquely identify a
 * particular cube. A valid HWIDs never contains 0xFF bytes.
 */

#define _SYS_HWID_BYTES         8
#define _SYS_HWID_BITS          64
#define _SYS_INVALID_HWID       ((uint64_t)-1)

/*
 * Filesystem
 */

#define _SYS_FS_VOL_GAME        0x4d47
#define _SYS_FS_VOL_LAUNCHER    0x4e4c

/// Opaque nonzero ID for a filesystem volume
typedef uint32_t _SYSVolumeHandle;      

/*
 * RFC4122 compatible UUIDs.
 *
 * These are used in game metadata, to uniquely identify a particular
 * binary. They are stored in network byte order, with field names compatible
 * with RFC4122.
 */

struct _SYSUUID {
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


#ifdef __cplusplus
}  // extern "C"
#endif

#endif
