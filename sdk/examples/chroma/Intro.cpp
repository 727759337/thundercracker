/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * timer for the game
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "Intro.h"
#include "string.h"
#include "assets.gen.h"
//#include "audio.gen.h"
#include "game.h"

const float Intro::INTRO_ARROW_TIME = 0.4f;
const float Intro::INTRO_TIMEREXPANSION_TIME = 0.5f;
const float Intro::INTRO_BALLEXPLODE_TIME = 0.5f;


Intro::Intro()
{
    m_fTimer = 0.0f;
}


void Intro::Reset( bool ingamereset)
{
    if( ingamereset )
    {
        m_fTimer = INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME;
        Game::Inst().playSound(glom_delay);
    }
    else
        m_fTimer = 0.0f;
}


bool Intro::Update( float dt )
{
    if( m_fTimer <= INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME && m_fTimer + dt > INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME )
        Game::Inst().playSound(glom_delay);

    m_fTimer += dt;

    if( m_fTimer > INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME + INTRO_BALLEXPLODE_TIME )
    {
        Game::Inst().setState( Game::STATE_PLAYING );
        return false;
    }

    return true;
}

const Sifteo::PinnedAssetImage *ARROW_SPRITES[ Intro::NUM_ARROWS ] =
{
    &ArrowUp,
    &ArrowLeft,
    &ArrowDown,
    &ArrowRight,
};

Vec2 STARTPOS[ Intro::NUM_ARROWS ] = {
    Vec2( 64 - ArrowUp.width * 8 / 2, 64 - ArrowUp.height * 8 / 2 ),
    Vec2( 64 - ArrowLeft.width * 8 / 2, 64 - ArrowLeft.height * 8 / 2 ),
    Vec2( 64 - ArrowDown.width * 8 / 2, 64 - ArrowDown.height * 8 / 2 ),
    Vec2( 64 - ArrowRight.width * 8 / 2, 64 - ArrowRight.height * 8 / 2 ),
};


Vec2 ENDPOS[ Intro::NUM_ARROWS ] = {
    Vec2( 64 - ArrowUp.width * 8 /2 , 0 ),
    Vec2( 0, 64 - ArrowLeft.height * 8 / 2  ),
    Vec2( 64 - ArrowDown.width * 8 / 2, 128 - ArrowDown.height * 8 ),
    Vec2( 128 - ArrowRight.width * 8, 64 - ArrowRight.height * 8 / 2 ),
};

//return whether we touched bg1 or not
bool Intro::Draw( TimeKeeper &timer, BG1Helper &bg1helper, VidMode_BG0_SPR_BG1 &vid, CubeWrapper *pWrapper )
{
    vid.clear(GemEmpty.tiles[0]);

    if( m_fTimer < INTRO_ARROW_TIME )
    {
        float timePercent = m_fTimer / INTRO_ARROW_TIME;

        //arrow sprites
        for( int i = 0; i < NUM_ARROWS; i++ )
        {
            Vec2 pos = LerpPosition( STARTPOS[i], ENDPOS[i], timePercent );
            vid.resizeSprite(i, ARROW_SPRITES[i]->width*8, ARROW_SPRITES[i]->height*8);
            vid.setSpriteImage(i, *ARROW_SPRITES[i], 0);
            vid.moveSprite(i, pos.x, pos.y);
        }
    }
    else if( m_fTimer < INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME )
    {
        for( int i = 0; i < NUM_ARROWS; i++ )
        {
            vid.resizeSprite(i, 0, 0);
        }

        //charge up timers
        float amount = ( m_fTimer - INTRO_ARROW_TIME ) / INTRO_TIMEREXPANSION_TIME;
        timer.DrawMeter( amount, bg1helper );
        return true;
    }
    else
    {
        int baseFrame = ( m_fTimer - ( INTRO_ARROW_TIME + INTRO_TIMEREXPANSION_TIME ) ) / INTRO_BALLEXPLODE_TIME * NUM_TOTAL_EXPLOSION_FRAMES;

        for( int i = 0; i < CubeWrapper::NUM_ROWS; i++ )
        {
            for( int j = 0; j < CubeWrapper::NUM_COLS; j++ )
            {
                GridSlot *pSlot = pWrapper->GetSlot( i, j );
                int frame = baseFrame;

                //middle ones are first
                if( i >= 1 && i < 3 && j >= 1 && j < 3 )
                    frame++;
                //corner ones are last
                if( frame > 0 && ( i == 0 || i == 3 ) && ( j == 0 || j == 3 ) )
                    frame--;

                pSlot->DrawIntroFrame( vid, frame );
            }
        }
    }

    return false;
}



Vec2 Intro::LerpPosition( Vec2 &start, Vec2 &end, float timePercent )
{
    Vec2 result( start.x + ( end.x - start.x ) * timePercent, start.y + ( end.y - start.y ) * timePercent );

    return result;
}
