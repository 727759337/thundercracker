/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * timer for timer mode
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _TIMEKEEPER_H
#define _TIMEKEEPER_H

#include <sifteo.h>

using namespace Sifteo;

class TimeKeeper
{
public:
    static const float TIME_INITIAL;
    //how long does it take the dot to go around?
    static const float TIMER_SPRITE_PERIOD;
    static const float TIMER_LOW_SPRITE_PERIOD;

    static const int TIMER_STEMS = 14;
    static const unsigned int BLINK_OFF_FRAMES = 7;
    static const unsigned int BLINK_ON_FRAMES = 10;
    static const unsigned int TIMER_POS = 6;
    static const unsigned int TIMER_SPRITE_POS = 48;
    static const unsigned int TIMER_SPRITE_NUM_ID = 0;

	TimeKeeper();

	void Reset();
    void Draw( TileBuffer<16, 16> &bg1buffer, VideoBuffer &vid );
    void Update( TimeDelta dt );
	void Init( SystemTime t );
	
    void DrawMeter( float amount, TileBuffer<16, 16> &bg1buffer, VideoBuffer &vid );
	float getTime() const { return m_fTimer; }

private:
	float m_fTimer;
    //unsigned int m_blinkCounter;
};

#endif
