/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _SCRIPT_H
#define _SCRIPT_H

extern "C" {
#   include "lua/lua.h"
#   include "lua/lauxlib.h"
#   include "lua/lualib.h"
}

#include "lunar.h"
#include "logger.h"
#include "tile.h"
#include "imagestack.h"

namespace Stir {

class Group;
class Image;


/*
 * Script --
 *
 *    STIR is configured via a script file that defines all assets.
 *    This object manages the execution of a STIR script using an
 *    embedded Lua interpreter.
 */

class Script {
public:
    Script(Logger &l);
    ~Script();

    bool run(const char *filename);

    bool addOutput(const char *filename);
    void setVariable(const char *key, const char *value);

 private:
    lua_State *L;
    Logger &log;

    bool anyOutputs;
    const char *outputHeader;
    const char *outputSource;
    const char *outputProof;

    friend class Group;
    friend class Image;

    bool luaRunFile(const char *filename);
    static bool matchExtension(const char *filename, const char *ext);

    static Script *getInstance(lua_State *L);

    // Utilities for foolproof table argument unpacking
    static bool argBegin(lua_State *L, const char *className);
    static bool argMatch(lua_State *L, const char *arg);
    static bool argMatch(lua_State *L, lua_Integer arg);
    static bool argEnd(lua_State *L);
};


/*
 * Group --
 *
 *    Scriptable object for an asset group. Asset groups are
 *    independently-loadable sets of assets which are optimized and
 *    deployed as a single unit.
 */

class Group {
public:
    static const char className[];
    static Lunar<Group>::RegType methods[];

    Group(lua_State *L);

    lua_Number getQuality() const {
	return quality;
    }

    void setDefault(lua_State *L);
    static Group *getDefault(lua_State *L);

private:
    lua_Number quality;
    TilePool pool;
};


/*
 * Image --
 *
 *    Scriptable object for an image asset. Images are a very generic
 *    asset type, which can refer to a stack of one or more individual
 *    bitmaps which are converted to tiles either using a map or using
 *    linear assignment.
 */

class Image {
public:
    static const char className[];
    static Lunar<Image>::RegType methods[];

    Image(lua_State *L);

 private:
    Group *mGroup;
    ImageStack mImages;
    lua_Number mQuality;

    int width(lua_State *L);
    int height(lua_State *L);
    int frames(lua_State *L);
    int quality(lua_State *L);
    int group(lua_State *L);
};


};  // namespace Stir

#endif
