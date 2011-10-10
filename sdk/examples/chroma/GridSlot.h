/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _GRIDSLOT_H
#define _GRIDSLOT_H

#include <sifteo.h>

using namespace Sifteo;

//space for a gem
class GridSlot
{
public:
	static const int NUM_COLORS = 4;
	static const AssetImage *TEXTURES[ NUM_COLORS ];

	static const float MARK_SPREAD_DELAY = 0.33f;
	static const float MARK_BREAK_DELAY = 0.67f;
	static const float MARK_EXPLODE_DELAY = 0.16f;

	typedef enum 
	{
		STATE_LIVING,
		STATE_MARKED,
		STATE_EXPLODING,
		STATE_SHOWINGSCORE,
		STATE_GONE,
	} SLOT_STATE;

	GridSlot();

	const AssetImage &GetTexture() const;
	//draw self on given vid at given vec
	void Draw( VidMode_BG0 &vid, const Vec2 &vec );
	void Update(float t);
	bool isAlive() const { return m_state == STATE_LIVING; }
	bool isEmpty() const { return m_state == STATE_GONE; }
	void setEmpty() { m_state = STATE_GONE; }
	float getValue() const { return m_value; }

	void mark();
	void spread_mark();
	void explode();
	void die();
private:
	SLOT_STATE m_state;
	unsigned int m_color;
	unsigned int m_value;
	float m_eventTime;
};


#endif