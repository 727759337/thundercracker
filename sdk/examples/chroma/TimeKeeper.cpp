/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * timer for the game
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "TimeKeeper.h"
#include "Game.h"
#include "assets.gen.h"
//#include "audio.gen.h"

const float TimeKeeper::TIME_INITIAL = 60.0f;
const float TimeKeeper::TIMER_SPRITE_PERIOD = 2.0f;
const float TimeKeeper::TIMER_LOW_SPRITE_PERIOD = 0.3f;


TimeKeeper::TimeKeeper()
{
    Reset();
}


void TimeKeeper::Reset()
{
	m_fTimer = TIME_INITIAL;
    //m_blinkCounter = 0;
}

void TimeKeeper::Draw( TileBuffer<16, 16> &bg1buffer, VideoBuffer &vid )
{
	//find out what proportion of our timer is left, then multiply by number of tiles
	float fTimerProportion = m_fTimer / TIME_INITIAL;

    DrawMeter( fTimerProportion, bg1buffer, vid );
}


void TimeKeeper::Update( TimeDelta dt )
{
	m_fTimer -= dt;
    //m_blinkCounter++;
}


void TimeKeeper::Init( SystemTime t )
{
	Reset();
}


void TimeKeeper::DrawMeter( float amount, TileBuffer<16, 16> &bg1buffer, VideoBuffer &vid )
{
    if( amount > 1.0f )
        amount = 1.0f;

    int numStems = TIMER_STEMS * amount;

    /*if( numStems <= 2 && m_blinkCounter < BLINK_OFF_FRAMES )
    {
        vid.resizeSprite(0, 0, 0);
        return;
    }*/

    if( numStems <= 2 )
    {
        float spritePerc = 1.0f - fmod( m_fTimer, TIMER_LOW_SPRITE_PERIOD ) / TIMER_LOW_SPRITE_PERIOD;
        unsigned int spriteframe = spritePerc * ( timerLow.numFrames() + 1 );

        if( spriteframe >= timerLow.numFrames() )
            spriteframe = timerLow.numFrames() - 1;

        vid.sprites[TIMER_SPRITE_NUM_ID].setImage(timerLow, spriteframe);
        vid.sprites[TIMER_SPRITE_NUM_ID].move(TIMER_SPRITE_POS, TIMER_SPRITE_POS);
    }
    else
    {
        //figure out what frame we're on
        float spritePerc = 1.0f - fmod( m_fTimer, TIMER_SPRITE_PERIOD ) / TIMER_SPRITE_PERIOD;
        unsigned int spriteframe = spritePerc * ( timerSprite.numFrames() + 1 );

        if( spriteframe >= timerSprite.numFrames() )
            spriteframe = timerSprite.numFrames() - 1;

        vid.sprites[TIMER_SPRITE_NUM_ID].setImage(timerSprite, spriteframe);
        vid.sprites[TIMER_SPRITE_NUM_ID].move(TIMER_SPRITE_POS, TIMER_SPRITE_POS);
    }


    if( numStems > 0 )
    {
        bg1buffer.image( vec( TIMER_POS, TIMER_POS ), timerStem, TIMER_STEMS - numStems );
    }

    /*if( numStems <= 2 && m_blinkCounter - BLINK_OFF_FRAMES >= BLINK_ON_FRAMES )
    {
        m_blinkCounter = 0;
        Game::Inst().playSound(timer_blink);
    }*/
}
