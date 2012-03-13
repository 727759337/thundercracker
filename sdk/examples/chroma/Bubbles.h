/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Handle bubble sprites
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _BUBBLES_H
#define _BUBBLES_H

#include <sifteo.h>

using namespace Sifteo;

class CubeWrapper;

class Bubble
{
public:
    static const int SPAWN_EXTENT = 32;
    static const float BUBBLE_LIFETIME;

    Bubble();
    void Spawn();
    void Disable();
    void Update( float dt );
    void Draw( VidMode_BG0_SPR_BG1 &vid, int index );
    inline bool isAlive() const { return m_fTimeAlive >= 0.0f; }

private:
    Float2 m_pos;
    float m_fTimeAlive;
};

class BubbleSpawner
{
public:
    static const unsigned int MAX_BUBBLES = 4;
    static const float MIN_SHORT_SPAWN;
    static const float MAX_SHORT_SPAWN;
    static const float MIN_LONG_SPAWN;
    static const float MAX_LONG_SPAWN;
    static const float SHORT_PERIOD_CHANCE;
    static const unsigned int BUBBLE_SPRITEINDEX = 4;

    BubbleSpawner();
    void Reset();
    void Update( float dt );
    void Draw( VidMode_BG0_SPR_BG1 &vid );
private:
    Bubble m_aBubbles[MAX_BUBBLES];
    float m_fTimeTillSpawn;
};


#endif //_BUBBLES_H
