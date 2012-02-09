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

const float Intro::READYSETGO_BANNER_TIME = 0.9f;

const float STATE_TIMES[ Intro::STATE_CNT ] = {
    //STATE_ARROWS,
    0.4f,
    //STATE_TIMERGROWTH,
    0.5f,
    //STATE_BALLEXPLOSION,
    0.5f,
    //STATE_READY,
    1.0f,
    //STATE_SET,
    1.0f,
    //STATE_GO,
    1.0f,
};


Intro::Intro()
{
    m_fTimer = 0.0f;
    m_state = STATE_ARROWS;
}


void Intro::Reset( bool ingamereset)
{
    if( ingamereset )
    {
        m_state = STATE_BALLEXPLOSION;
        Game::Inst().playSound(glom_delay);
    }
    else
        m_state = STATE_ARROWS;

    m_fTimer = 0.0f;
}


bool Intro::Update( float dt, Banner &banner )
{
    m_fTimer += dt;

    if( m_fTimer >= STATE_TIMES[ m_state ] )
    {
        m_state = (IntroState)( (int)m_state + 1 );
        m_fTimer = 0.0f;

        //special cases
        switch( m_state )
        {
            case STATE_BALLEXPLOSION:
                Game::Inst().playSound(glom_delay);
                break;
            case STATE_READY:
                if( Game::Inst().getState() == Game::STATE_PLAYING )
                {
                    if( Game::Inst().getMode() == Game::MODE_SHAKES )
                    {
                        String<16> buf;
                        buf << "Level " << Game::Inst().getDisplayedLevel();
                        banner.SetMessage( buf, READYSETGO_BANNER_TIME );
                    }
                    return false;
                }
                else if( Game::Inst().getMode() == Game::MODE_SHAKES )
                    banner.SetMessage( "Clear the cubes!", READYSETGO_BANNER_TIME );
                else
                    banner.SetMessage( "60 seconds", READYSETGO_BANNER_TIME );
                break;
            case STATE_SET:
                if( Game::Inst().getMode() == Game::MODE_SHAKES )
                    return false;
                banner.SetMessage( "Ready", READYSETGO_BANNER_TIME );
                break;
            case STATE_GO:
                banner.SetMessage( "Go!", READYSETGO_BANNER_TIME );
                break;
            case STATE_CNT:
                Game::Inst().setState( Game::STATE_PLAYING );
                return false;
            default:
                break;
        }
    }

    banner.Update( dt );

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
    float timePercent = m_fTimer / STATE_TIMES[ m_state ];

    switch( m_state )
    {
        case STATE_ARROWS:
        {
            vid.clear(GemEmpty.tiles[0]);

            //arrow sprites
            for( int i = 0; i < NUM_ARROWS; i++ )
            {
                Vec2 pos = LerpPosition( STARTPOS[i], ENDPOS[i], timePercent );
                vid.resizeSprite(i, ARROW_SPRITES[i]->width*8, ARROW_SPRITES[i]->height*8);
                vid.setSpriteImage(i, *ARROW_SPRITES[i], 0);
                vid.moveSprite(i, pos.x, pos.y);
            }
            break;
        }
        case STATE_TIMERGROWTH:
        {
            vid.clear(GemEmpty.tiles[0]);

            for( int i = 0; i < NUM_ARROWS; i++ )
            {
                vid.resizeSprite(i, 0, 0);
            }

            //charge up timers
            timer.DrawMeter( timePercent, bg1helper );
            return true;
        }
        case STATE_BALLEXPLOSION:
        {
            int baseFrame = timePercent * NUM_TOTAL_EXPLOSION_FRAMES;

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
            break;
        }
        default:
            pWrapper->getBanner().Draw( bg1helper );
            return true;
    }

    return false;
}



Vec2 Intro::LerpPosition( Vec2 &start, Vec2 &end, float timePercent )
{
    Vec2 result( start.x + ( end.x - start.x ) * timePercent, start.y + ( end.y - start.y ) * timePercent );

    return result;
}
