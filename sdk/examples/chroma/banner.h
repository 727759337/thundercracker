/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * message banners that show up to display score or game info
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _BANNER_H
#define _BANNER_H

#include <sifteo.h>

using namespace Sifteo;

class Banner
{
public:
	static const int BANNER_WIDTH = 16;
	static const int BANNER_ROWS = 4;
	static const int MAX_BANNER_LENGTH = 16;

	Banner();

	void Draw( Cube &cube );
	void Update(float t);
private:
	char m_Msg[MAX_BANNER_LENGTH];
	float m_fStartTime;
};

#endif