/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * message banners that show up to display score or game info
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "banner.h"
#include "game.h"
#include "assets.gen.h"


const float Banner::DEFAULT_FADE_DELAY = 2.0f;
const float Banner::SCORE_TIME = 1.0f;

Banner::Banner()
{
    m_tiles = 0;
}


void Banner::Draw( TileBuffer<16, 16> &bg1buffer )
{
    int iLen = m_Msg.size();
    if( iLen == 0 )
		return;

    if( m_tiles == 0 )
        return;

    //bg1buffer.image( vec( 0, 6 ), BannerImg );
    bg1buffer.image( vec<unsigned>( CENTER_PT - m_tiles, 6 ), vec<unsigned>( CENTER_PT - m_tiles, 0 ), BannerImg, vec<unsigned>( m_tiles * 2, BANNER_ROWS ) );

    int iStartXTile = ( BANNER_WIDTH - iLen ) / 2;

    for( int i = 0; i < iLen; i++ )
    {
        int iOffset = iStartXTile + i;

        bg1buffer.image( vec( iOffset, 7 ), Font, m_Msg[i] - ' ' );
    }
}


void Banner::Update(SystemTime t)
{
    int iLen = m_Msg.size();
    if( iLen > 0 )
	{
		if( t > m_endTime )
		{
            m_Msg.clear();
            m_endTime = SystemTime();
		}
        m_tiles++;

        if( m_tiles > CENTER_PT )
            m_tiles = CENTER_PT;
	}
}


void Banner::SetMessage( const char *pMsg, float fTime )
{
    m_Msg = pMsg;
    float msgTime = fTime;
    m_endTime = SystemTime::now() + msgTime;
    m_tiles = 0;
}


bool Banner::IsActive() const
{
	return !m_Msg.empty();
}


void Banner::DrawScore( TileBuffer<16, 16> &bg1buffer, const Int2 &pos, Banner::Anchor anchor, int score )
{
    String<16> buf;
    buf << score;

    int iLen = buf.size();
    if( iLen == 0 )
        return;

    int offset;
    switch( anchor )
    {
		default:
        case LEFT:
        {
            // "pos" is the position of the leftmost tile in our score
            offset = 0;
            break;
        }
        
        case CENTER:
        {
            // "pos" is the center tile in our score
            offset = -iLen / 2;
            break;
        }
        
        case RIGHT:
        {
            // "pos" is the position of the rightmost tile
            offset = -iLen + 1;
            break;
        }
    }

    /*if( frame >= (int)FloatingScore::NUM_POINTS_FRAMES )
        frame = (int)FloatingScore::NUM_POINTS_FRAMES - 1;*/

    for( int i = 0; i < iLen; i++ )
    {
        /*if( frame >= 0 )
            bg1buffer.image( vec( pos.x + i + offset, pos.y ), PointFont, ( buf[i] - '0' ) * FloatingScore::NUM_POINTS_FRAMES + frame );
        else*/
            bg1buffer.image( vec( pos.x + i + offset, pos.y ), BannerPointsWhite, buf[i] - '0' );
    }
}
