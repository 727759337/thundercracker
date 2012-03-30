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
    static const unsigned int BANNER_WIDTH = 16;
    static const unsigned int CENTER_PT = 8;
	static const int BANNER_ROWS = 4;
    static const float DEFAULT_FADE_DELAY;
    static const float SCORE_TIME;

    enum Anchor {
        LEFT,
        CENTER,
        RIGHT,
    };

	Banner();

    void Draw( BG1Helper &bg1helper );
    void Update( SystemTime t );

    void SetMessage( const char *pMsg, float fTime = DEFAULT_FADE_DELAY );
	bool IsActive() const;

    static void DrawScore( BG1Helper &bg1helper, const Int2 &pos, Anchor anchor, int score );

private:
    String<BANNER_WIDTH + 1> m_Msg;
    SystemTime m_endTime;
    //how many tiles of the banner to show
    unsigned int m_tiles;
};

#endif
