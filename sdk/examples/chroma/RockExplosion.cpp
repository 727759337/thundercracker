/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Logic for handling rock explosions
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "assets.gen.h"
#include "RockExplosion.h"


RockExplosion::RockExplosion()
{
    Reset();
}


void RockExplosion::Reset()
{
    m_pos.set( -1, -1 );
    m_animFrame = 0;
}


void RockExplosion::Update( float dt )
{
    if( m_pos.x >= 0 )
    {
        //for now, try frame at a time
        m_animFrame++;

        if( m_animFrame >= rock_explode.frames )
        {
            Reset();
        }
    }
}


void RockExplosion::Draw( VidMode_BG0_SPR_BG1 &vid, int spriteindex )
{
    if( m_pos.x >= 0 )
    {
        vid.resizeSprite(spriteindex, rock_explode.width*8, rock_explode.height*8);
        vid.setSpriteImage(spriteindex, rock_explode, m_animFrame);
        vid.moveSprite(spriteindex, m_pos.x, m_pos.y);
    }
    else
        vid.resizeSprite(spriteindex, 0, 0);
}


