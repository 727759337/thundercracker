/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * handles glimmering balls on idle
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "Glimmer.h"
#include "assets.gen.h"
//#include "audio.gen.h"
//#include "sprite.h"
#include "game.h"
#include "cubewrapper.h"


Int2 GLIMMER_ORDER_1[] = { vec( 0, 0 ) };
Int2 GLIMMER_ORDER_2[] = { vec( 1, 0 ), vec( 0, 1 ) };
Int2 GLIMMER_ORDER_3[] = { vec( 2, 0 ), vec( 1, 1 ), vec( 0, 2 ) };
Int2 GLIMMER_ORDER_4[] = { vec( 3, 0 ), vec( 1, 2 ), vec( 2, 1 ), vec( 0, 3 ) };
Int2 GLIMMER_ORDER_5[] = { vec( 3, 1 ), vec( 2, 2 ), vec( 1, 3 ) };
Int2 GLIMMER_ORDER_6[] = { vec( 3, 2 ), vec( 2, 3 ) };
Int2 GLIMMER_ORDER_7[] = { vec( 3, 3 ) };

//list of locations to glimmer in order
Int2 *Glimmer::GLIMMER_ORDER[NUM_GLIMMER_GROUPS] =
{
    GLIMMER_ORDER_1,
    GLIMMER_ORDER_2,
    GLIMMER_ORDER_3,
    GLIMMER_ORDER_4,
    GLIMMER_ORDER_5,
    GLIMMER_ORDER_6,
    GLIMMER_ORDER_7,
};


Glimmer::Glimmer()
{
    Reset();
}


void Glimmer::Reset()
{
    m_frame = 0;
    m_group = 0;
    //Game::Inst().playSound(glimmer_fx_03);
}


void Glimmer::Update( float dt, CubeWrapper *pWrapper )
{
    if( m_group == 0 && m_frame == 0 )
        Game::Inst().playSound(glimmer_fx_03);

    if( m_group < NUM_GLIMMER_GROUPS )
    {
        m_frame++;

        if( m_frame >= GlimmerImg.frames )
        {
            m_frame = 0;
            m_group++;

            if( m_group >= NUM_GLIMMER_GROUPS )
                pWrapper->setNeedFlush();
            //Game::Inst().playSound(glimmer_fx_03);
        }
    }
}


int Glimmer::NUM_PER_GROUP[NUM_GLIMMER_GROUPS] =
{
  1,
    2,
    3,
    4,
    3,
    2,
    1
};


void Glimmer::Draw( BG1Helper &bg1helper, CubeWrapper *pWrapper )
{
    //old sprite version
    /*if( m_group >= NUM_GLIMMER_GROUPS )
    {
        for( int i = 0; i < MAX_GLIMMERS; i++ )
            resizeSprite(cube, i, 0, 0);
        return;
    }

    for( int i = 0; i < MAX_GLIMMERS; i++ )
    {
        if( i < NUM_PER_GROUP[ m_group ] )
        {
            Int2 &loc = GLIMMER_ORDER[ m_group ][i];
            resizeSprite(cube, i, GlimmerImg.width*8, GlimmerImg.height*8);
            moveSprite(cube, i, loc.x * 32, loc.y * 32);
        }
        else
            resizeSprite(cube, i, 0, 0);
        setSpriteImage(cube, i, GlimmerImg, m_frame);

    }*/

    //bg1 version
    for( int i = 0; i < MAX_GLIMMERS; i++ )
    {
        if( i < NUM_PER_GROUP[ m_group ] )
        {
            Int2 &loc = GLIMMER_ORDER[ m_group ][i];
            GridSlot *pSlot = pWrapper->GetSlot( loc.x, loc.y );

            if( pSlot->isAlive() )
            {
                if( pSlot->IsFixed() )
                {
                    if( pSlot->getMultiplier() <= 1 )
                        bg1helper.DrawAsset( vec( loc.y * 4, loc.x * 4 ), FixedGlimmer, m_frame );
                }
                else if( pSlot->getColor() != GridSlot::ROCKCOLOR )
                    bg1helper.DrawAsset( vec( loc.y * 4, loc.x * 4 ), GlimmerImg, m_frame );
            }
        }
    }

    pWrapper->setNeedFlush();
}


