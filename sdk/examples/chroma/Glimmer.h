/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * timer for timer mode
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _Glimmer_H
#define _Glimmer_H

#include <sifteo.h>

using namespace Sifteo;

class CubeWrapper;

class Glimmer
{
public:
    static const unsigned int NUM_GLIMMER_GROUPS = 7;
    //max at a time
    static const int MAX_GLIMMERS = 4;
    //list of locations to glimmer in order
    static Int2 *GLIMMER_ORDER[NUM_GLIMMER_GROUPS];
    static int NUM_PER_GROUP[NUM_GLIMMER_GROUPS];

    Glimmer();
    void Reset();
    void Update( float dt, CubeWrapper *pWrapper );
    void Draw( TileBuffer<16, 16> &bg1buffer, CubeWrapper *pWrapper ) __attribute__ ((noinline));
    inline void Stop() { m_group = NUM_GLIMMER_GROUPS; }
    inline bool IsActive() const { return m_group < NUM_GLIMMER_GROUPS; }
	
private:
    unsigned int m_frame;
    unsigned int m_group;
    //did we change the mask?
    bool m_bNeedFinish;
};

#endif
