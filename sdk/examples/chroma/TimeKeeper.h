/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * timer for timer mode
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _TIMEKEEPER_H
#define _TIMEKEEPER_H

#include <sifteo.h>
#include "BG1Helper.h"

using namespace Sifteo;

class TimeKeeper
{
public:
	static const float TIME_INITIAL = 60.0f;
	static const float TIME_RETURN_PER_GEM = 1.0f;
	static const int TIMER_TILES = 8;

	TimeKeeper();

	void Reset();
	void Draw( BG1Helper &bg1helper );
	void Update( float t );
	void Init( float t );
	
	float getTime() const { return m_fTimer; }
	void AddTime( int numGems );

private:
	float m_fTimer;
	float m_fLastTime;
};

#endif