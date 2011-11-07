/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "GridSlot.h"
#include "game.h"
#include "assets.gen.h"
#include "utils.h"

const AssetImage *GridSlot::TEXTURES[ GridSlot::NUM_COLORS ] = 
{
	&Gem0,
	&Gem1,
	&Gem2,
	&Gem3,
};

GridSlot::GridSlot() : 
	m_state( STATE_LIVING ),
	m_eventTime( 0.0f ),
	m_score( 0 )
{
	//TEST randomly make some empty ones
	/*if( Game::Rand(100) > 50 )
		m_state = STATE_LIVING;
	else
		m_state = STATE_GONE;*/
	m_color = Game::Rand(NUM_COLORS);
}


void GridSlot::Init( CubeWrapper *pWrapper, unsigned int row, unsigned int col )
{
	m_pWrapper = pWrapper;
	m_row = row;
	m_col = col;
	m_state = STATE_LIVING;
}


const AssetImage &GridSlot::GetTexture() const
{
	return *TEXTURES[ m_color ];
}

//draw self on given vid at given vec
void GridSlot::Draw( VidMode_BG0 &vid, const Vec2 &vec )
{
	switch( m_state )
	{
		case STATE_LIVING:
		{
			const AssetImage &tex = GetTexture();
			vid.BG0_drawAsset(vec, tex, 0);
			break;
		}
		case STATE_MARKED:
		{
			const AssetImage &tex = GetTexture();
			vid.BG0_drawAsset(vec, tex, 1);
			break;
		}
		case STATE_EXPLODING:
		{
			const AssetImage &tex = GetTexture();
			vid.BG0_drawAsset(vec, tex, 4);
			break;
		}
		case STATE_SHOWINGSCORE:
		{
			char aStr[2];
			sprintf( aStr, "%d", m_score );
			vid.BG0_drawAsset(vec, GemEmpty, 0);
			vid.BG0_text(Vec2( vec.x + 1, vec.y + 1 ), Font, aStr);
			break;
		}
		case STATE_GONE:
		{
			vid.BG0_drawAsset(vec, GemEmpty, 0);
			break;
		}
		default:
			break;
	}
	
}


void GridSlot::Update(float t)
{
	switch( m_state )
	{
		case STATE_MARKED:
		{
			if( t - m_eventTime > MARK_SPREAD_DELAY )
                spread_mark();
            if( t - m_eventTime > MARK_BREAK_DELAY )
                explode();
			break;
		}
		case STATE_EXPLODING:
		{
			spread_mark();
			if( t - m_eventTime > MARK_EXPLODE_DELAY )
                die();
			break;
		}
		case STATE_SHOWINGSCORE:
		{
			if( t - m_eventTime > SCORE_FADE_DELAY )
                m_state = STATE_GONE;
			break;
		}
		default:
			break;
	}
}


void GridSlot::mark()
{
	m_state = STATE_MARKED;
	m_eventTime = System::clock();
}


void GridSlot::spread_mark()
{
	if( m_state == STATE_MARKED || m_state == STATE_EXPLODING )
	{
		markNeighbor( m_row - 1, m_col );
		markNeighbor( m_row, m_col - 1 );
		markNeighbor( m_row + 1, m_col );
		markNeighbor( m_row, m_col + 1 );
	}
}

void GridSlot::explode()
{
	m_state = STATE_EXPLODING;
	m_eventTime = System::clock();
}

void GridSlot::die()
{
	m_state = STATE_SHOWINGSCORE;
	m_score = Game::Inst().getIncrementScore();
	m_eventTime = System::clock();
}


void GridSlot::markNeighbor( int row, int col )
{
	//find my neighbor and see if we match
	GridSlot *pNeighbor = m_pWrapper->GetSlot( row, col );

	//PRINT( "pneighbor = %p", pNeighbor );
	//PRINT( "color = %d", pNeighbor->getColor() );
	if( pNeighbor && pNeighbor->isAlive() && pNeighbor->getColor() == m_color )
		pNeighbor->mark();
}