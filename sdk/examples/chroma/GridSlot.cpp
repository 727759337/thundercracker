/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "GridSlot.h"
#include "game.h"
#include "assets.gen.h"
//#include "audio.gen.h"
#include "utils.h"


const float GridSlot::MARK_SPREAD_DELAY = 0.333333f;
const float GridSlot::MARK_BREAK_DELAY = 0.666666f;
const float GridSlot::MARK_EXPLODE_DELAY = 0.16666666f;
const float GridSlot::EXPLODE_FRAME_LEN = ( GridSlot::MARK_BREAK_DELAY - GridSlot::MARK_SPREAD_DELAY ) / (float) GridSlot::NUM_EXPLODE_FRAMES;
const unsigned int GridSlot::NUM_ROLL_FRAMES = 16 * GridSlot::NUM_FRAMES_PER_ROLL_ANIM_FRAME;
//const unsigned int GridSlot::NUM_IDLE_FRAMES = 4 * GridSlot::NUM_FRAMES_PER_IDLE_ANIM_FRAME;
const float GridSlot::MULTIPLIER_LIGHTNING_PERIOD = 0.75f;
const float GridSlot::MULTIPLIER_NUMBER_PERIOD = 1.0f;
//what proportion of MULTIPLIER_NUMBER_PERIOD is the number displayed
const float GridSlot::MULTIPLIER_NUMBER_PERCENTON = 0.7f;


const FlatAssetImage *GridSlot::TEXTURES[ GridSlot::NUM_COLORS ] =
{
    &Gem0,
    &Gem1,
    &Gem2,
    &Gem3,
	&Gem4,
	&Gem5,
	&Gem6,
    &Gem7,
};

const FlatAssetImage *GridSlot::EXPLODINGTEXTURES[ GridSlot::NUM_COLORS ] =
{
    &ExplodeGem0,
    &ExplodeGem1,
    &ExplodeGem2,
    &ExplodeGem3,
    &ExplodeGem4,
    &ExplodeGem5,
    &ExplodeGem6,
    &ExplodeGem7,
};


const FlatAssetImage *GridSlot::FIXED_TEXTURES[ GridSlot::NUM_COLORS ] =
{
    &FixedGem0,
    &FixedGem1,
    &FixedGem2,
    &FixedGem3,
    &FixedGem4,
    &FixedGem5,
    &FixedGem6,
    &FixedGem7,
};

const FlatAssetImage *GridSlot::FIXED_EXPLODINGTEXTURES[ GridSlot::NUM_COLORS ] =
{
    &FixedExplode0,
    &FixedExplode1,
    &FixedExplode2,
    &FixedExplode3,
    &FixedExplode4,
    &FixedExplode5,
    &FixedExplode6,
    &FixedExplode7,
};


const FlatAssetImage *GridSlot::SPECIALTEXTURES[ NUM_SPECIALS ] =
{
    &hyperdot_idle,
    &rockdot,
    &rainball_idle
};



const FlatAssetImage *GridSlot::SPECIALEXPLODINGTEXTURES[ NUM_SPECIALS ] =
{
    &hyperdot_explode,
    &rockdot,
    &rainball_explode
};

//order of our frames
enum
{
    FRAME_REST,
    FRAME_N1,
    FRAME_N2,
    FRAME_N3,
    FRAME_NE1,
    FRAME_NE2,
    FRAME_NE3,
    FRAME_E1,
    FRAME_E2,
    FRAME_E3,
    FRAME_SE1,
    FRAME_SE2,
    FRAME_SE3,
    FRAME_S1,
    FRAME_S2,
    FRAME_S3,
    FRAME_SW1,
    FRAME_SW2,
    FRAME_SW3,
    FRAME_W1,
    FRAME_W2,
    FRAME_W3,
    FRAME_NW1,
    FRAME_NW2,
    FRAME_NW3,
};

//quantize our tilt state from [-128, 128] to [-3, 3], then offset to [0, 6]
//use those values to index into this lookup table to find which frame to render
//in row, column format
unsigned int TILTTOFRAMES[GridSlot::NUM_QUANTIZED_TILT_VALUES][GridSlot::NUM_QUANTIZED_TILT_VALUES] = {
    //most northward
    { FRAME_NW3, FRAME_NW3, FRAME_N3, FRAME_N3, FRAME_N3, FRAME_NE3, FRAME_NE3 },
    { FRAME_NW3, FRAME_NW2, FRAME_N2, FRAME_N2, FRAME_N2, FRAME_NE2, FRAME_NE3 },
    { FRAME_W3, FRAME_W2, FRAME_NW1, FRAME_N1, FRAME_NE1, FRAME_E2, FRAME_E3 },
    { FRAME_W3, FRAME_W2, FRAME_W1, FRAME_REST, FRAME_E1, FRAME_E2, FRAME_E3 },
    { FRAME_W3, FRAME_W2, FRAME_SW1, FRAME_S1, FRAME_SE1, FRAME_E2, FRAME_E3 },
    { FRAME_SW3, FRAME_SW2, FRAME_S2, FRAME_S2, FRAME_S2, FRAME_SE2, FRAME_SE3 },
    { FRAME_SW3, FRAME_SW3, FRAME_S3, FRAME_S3, FRAME_S3, FRAME_SE3, FRAME_SE3 },
};


GridSlot::GridSlot() : 
	m_state( STATE_GONE ),
    m_Movestate( MOVESTATE_STATIONARY ),
	m_eventTime(),
	m_score( 0 ),
	m_bFixed( false ),
    m_multiplier( 1 ),
	m_animFrame( 0 )
{
	m_color = Game::random.randrange(NUM_COLORS);
    m_lastFrameDir = vec( 0, 0 );
}


void GridSlot::Init( CubeWrapper *pWrapper, unsigned int row, unsigned int col )
{
	m_pWrapper = pWrapper;
	m_row = row;
	m_col = col;
	m_state = STATE_GONE;
    m_Movestate = MOVESTATE_STATIONARY;
    m_RockHealth = MAX_ROCK_HEALTH;
}

//bsetspawn to force spawning state, only used for timer respawning currently
void GridSlot::FillColor( unsigned int color, bool bSetSpawn )
{
    if( bSetSpawn )
    {
        m_state = STATE_SPAWNING;
        m_animFrame = 0;
    }
    else
        m_state = STATE_LIVING;

    ASSERT( m_color >= 0 && m_color < NUM_COLORS_INCLUDING_SPECIALS);

	m_color = color;
	m_bFixed = false;
    m_bWasRainball = false;
    m_bWasInfected = false;
    m_multiplier = 1;
    m_eventTime = SystemTime::now();

    if( color == ROCKCOLOR )
        m_RockHealth = MAX_ROCK_HEALTH;
}


bool GridSlot::matchesColor( unsigned int color ) const
{
    return isAlive() && ( getColor() == color || getColor() == GridSlot::RAINBALLCOLOR || getColor() == GridSlot::HYPERCOLOR );
}


const FlatAssetImage &GridSlot::GetTexture() const
{
	return *TEXTURES[ m_color ];
}


const FlatAssetImage &GridSlot::GetExplodingTexture() const
{
    if( IsFixed() )
        return *FIXED_EXPLODINGTEXTURES[ m_color ];
    else
        return *EXPLODINGTEXTURES[ m_color ];
}


const FlatAssetImage &GridSlot::GetSpecialTexture() const
{
    ASSERT( m_color >= NUM_COLORS && m_color < NUM_COLORS_INCLUDING_SPECIALS);

    return *SPECIALTEXTURES[ m_color - NUM_COLORS ];
}


const FlatAssetImage &GridSlot::GetSpecialExplodingTexture() const
{
    ASSERT( m_color >= NUM_COLORS && m_color < NUM_COLORS_INCLUDING_SPECIALS);

    return *SPECIALEXPLODINGTEXTURES[ m_color - NUM_COLORS ];
}


unsigned int GridSlot::GetSpecialFrame()
{
    if( m_color == ROCKCOLOR )
    {
        if( m_RockHealth > 0 )
            return MAX_ROCK_HEALTH - m_RockHealth;
        else
            return 0;
    }
    else
    {
        m_animFrame++;

        if( m_animFrame >= GetSpecialTexture().numFrames() )
            m_animFrame = 0;

        return m_animFrame;
    }
}


//draw self on given vid at given vec
void GridSlot::Draw( VideoBuffer &vid, TileBuffer<16, 16> &bg1buffer, Float2 &tiltState )
{
	Int2 vec = { m_col * 4, m_row * 4 };

    switch( m_state )
	{
        case STATE_SPAWNING:
        {
            DrawIntroFrame( vid, m_animFrame );
            break;
        }
		case STATE_LIVING:
		{
            if( IsSpecial() )
                vid.bg0.image(vec, GetSpecialTexture(), GetSpecialFrame() );
            else if( IsFixed() )
            {
                if( m_Movestate == MOVESTATE_FIXEDATTEMPT )
                {
                    vid.bg0.image(vec, *FIXED_TEXTURES[ m_color ], GetFixedFrame( m_animFrame ));
                }
                else
                {
                    vid.bg0.image(vec, *FIXED_TEXTURES[ m_color ]);

                    if( m_multiplier > 1 )
                    {
                        SystemTime t = SystemTime::now();
                        
                        unsigned int frame = t.cycleFrame(MULTIPLIER_LIGHTNING_PERIOD, mult_lightning.numFrames());
                        vid.sprites[MULT_SPRITE_ID].setImage(mult_lightning, frame);
                        vid.sprites[MULT_SPRITE_ID].move( m_col * 32, m_row * 32 );

                        //number on bg1
                        if( t.cyclePhase(MULTIPLIER_NUMBER_PERIOD) < MULTIPLIER_NUMBER_PERCENTON )
                        {
                            vid.sprites[MULT_SPRITE_NUM_ID].setImage(mult_numbers, m_multiplier - 2);
                            vid.sprites[MULT_SPRITE_NUM_ID].move( m_col * 32, m_row * 32 + 6 );
                        }
                    }
                }
            }
			else
			{
                switch( m_Movestate )
                {
                    case MOVESTATE_STATIONARY:
                    case MOVESTATE_PENDINGMOVE:
                    {
                        const FlatAssetImage &animtex = *TEXTURES[ m_color ];
                        unsigned int frame;
                        /*if( m_pWrapper->IsIdle() )
                            frame = GetIdleFrame();
                        else*/
                            frame = GetTiltFrame( tiltState, m_lastFrameDir );
                        vid.bg0.image(vec, animtex, frame);
                        break;
                    }
                    case MOVESTATE_MOVING:
                    {
                        Int2 curPos = m_curMovePos;

                        //PRINT( "drawing dot x=%d, y=%d\n", m_curMovePos.x, m_curMovePos.y );
                        if( IsSpecial() )
                            vid.bg0.image(curPos, GetSpecialTexture(), GetSpecialFrame());
                        else
                        {
                            const FlatAssetImage &tex = *TEXTURES[m_color];
                            vid.bg0.image(curPos, tex, GetRollingFrame( m_animFrame ));
                        }
                        break;
                    }
                    case MOVESTATE_FINISHINGMOVE:
                    case MOVESTATE_BUMPED:
                    {
                        if( IsSpecial() )
                            vid.bg0.image(vec, GetSpecialTexture(), GetSpecialFrame());
                        else
                        {
                            const FlatAssetImage &animtex = *TEXTURES[ m_color ];
                            vid.bg0.image(vec, animtex, m_animFrame);
                        }
                        break;
                    }
                    default:
                        ASSERT( 0 );
                }
			}
			break;
		}
		case STATE_MARKED:
        {
            if( m_color == HYPERCOLOR )
            {
                //vid.bg0.image(vec, GetSpecialTexture(), GetSpecialFrame() );
                const FlatAssetImage &exTex = GetSpecialExplodingTexture();

                vid.bg0.image(vec, exTex, GetSpecialFrame());
            }
            else
            {
                if( m_color == RAINBALLCOLOR )
                {
                    vid.bg0.image(vec, rainball_idle, 0);
                }
                else
                {
                    const FlatAssetImage &exTex = GetExplodingTexture();

                    unsigned int markFrame = m_bWasRainball ? 0 : m_animFrame;

                    vid.bg0.image(vec, exTex, markFrame);
                }

                if( m_bWasRainball || m_bWasInfected )
                {
                    const FlatAssetImage *pImg = 0;

                    if( m_bWasRainball )
                    {
                        pImg = &rainball_explode;
                    }
                    else if( m_bWasInfected )
                    {
                        pImg = &hyperdot_activation;
                        vec.x += 1;
                        vec.y += 1;
                    }

                    float timeDiff = SystemTime::now() - m_eventTime;
                    float perc = timeDiff / MARK_BREAK_DELAY;

                    //for some reason I'm seeing extremely small negative values at times.
                    if( perc >= 0.0f && perc < 1.0f )
                    {
                        //figure out frame based on mark break delay
                        unsigned int frame = pImg->numFrames() * perc;

                        bg1buffer.image( vec, *pImg, frame );
                    }
                }
            }
			break;
		}
		case STATE_EXPLODING:
		{
            /*if( IsSpecial() )
                vid.bg0.image(vec, GetSpecialTexture(), GetSpecialFrame());
            else*/
            {
                vid.bg0.image(vec, GemEmpty, 0);
                //const FlatAssetImage &exTex = GetExplodingTexture();
                //vid.bg0.image(vec, exTex, GridSlot::NUM_EXPLODE_FRAMES - 1);
            }
			break;
		}
        /*case STATE_SHOWINGSCORE:
		{
            if( m_score > 99 )
                m_score = 99;
            vid.bg0.image(vec, GemEmpty, 0);
            unsigned int fadeFrame = 0;

            float fadeTime = float(SystemTime::now() - m_eventTime) - START_FADING_TIME;

            if( fadeTime > 0.0f )
                fadeFrame =  ( fadeTime ) / FADE_FRAME_TIME;

            if( fadeFrame >= NUM_POINTS_FRAMES )
                fadeFrame = NUM_POINTS_FRAMES - 1;

            if( m_score > 9 )
                vid.bg0.image(vec( vec.x + 1, vec.y + 1 ), PointFont, m_score / 10 * NUM_POINTS_FRAMES + fadeFrame);
            vid.bg0.image(vec( vec.x + 2, vec.y + 1 ), PointFont, m_score % 10 * NUM_POINTS_FRAMES + fadeFrame);
			break;
		}
        case STATE_GONE:
		{
            vid.bg0.image(vec, GemEmpty, 0);
			break;
        }*/
		default:
			break;
	}
}


void GridSlot::Update(SystemTime t)
{
	switch( m_state )
	{
        case STATE_SPAWNING:
        {
            m_animFrame++;
            if( m_animFrame >= NUM_SPAWN_FRAMES )
            {
                m_animFrame = 0;
                m_state = STATE_LIVING;
            }
            break;
        }
        case STATE_LIVING:
        {
            switch( m_Movestate )
            {
                case MOVESTATE_MOVING:
                {
                    Int2 vDiff = vec( m_col * 4 - m_curMovePos.x, m_row * 4 - m_curMovePos.y );

                    if( vDiff.x != 0 )
                    {
                        m_curMovePos.x += ( vDiff.x / abs( vDiff.x ) );
                        //clear this out in update
                        m_pWrapper->QueueClear( m_curMovePos );

                        if( abs( vDiff.x ) == 1 )
                            Game::Inst().playSound(collide_02);
                    }
                    else if( vDiff.y != 0 )
                    {
                        m_curMovePos.y += ( vDiff.y / abs( vDiff.y ) );
                        //clear this out in update
                        m_pWrapper->QueueClear( m_curMovePos );

                        if( abs( vDiff.y ) == 1 )
                            Game::Inst().playSound(collide_02);
                    }
                    else
                    {
                        m_animFrame++;
                        if( m_animFrame >= NUM_ROLL_FRAMES )
                        {
                            m_Movestate = MOVESTATE_FINISHINGMOVE;
                            m_animFrame = GetRollingFrame( NUM_ROLL_FRAMES - 1 );
                        }
                    }

                    break;
                }
                case MOVESTATE_FINISHINGMOVE:
                {
                    //interpolate frames back to normal state
                    Float2 cubeDir = m_pWrapper->getTiltDir();
                    Int2 curDir;

                    GetTiltFrame( cubeDir, curDir );

                    if( m_lastFrameDir == curDir )
                        m_Movestate = MOVESTATE_STATIONARY;
                    else
                    {
                        if( curDir.x > m_lastFrameDir.x )
                            m_lastFrameDir.x++;
                        else if( curDir.x < m_lastFrameDir.x )
                            m_lastFrameDir.x--;
                        if( curDir.y > m_lastFrameDir.y )
                            m_lastFrameDir.y++;
                        else if( curDir.y < m_lastFrameDir.y )
                            m_lastFrameDir.y--;

                        m_animFrame = TILTTOFRAMES[ m_lastFrameDir.y ][ m_lastFrameDir.x ];
                    }

                    break;
                }
                case MOVESTATE_FIXEDATTEMPT:
                {
                    ASSERT( IsFixed() );
                    m_animFrame++;
                    if( m_animFrame / NUM_FRAMES_PER_FIXED_ANIM_FRAME >= NUM_FIXED_FRAMES )
                        m_Movestate = MOVESTATE_STATIONARY;

                    break;
                }
                case MOVESTATE_BUMPED:
                {
                    m_Movestate = MOVESTATE_FINISHINGMOVE;
                    break;
                }
                default:
                    break;
            }

            break;
        }
		case STATE_MARKED:
		{
			if( t - m_eventTime > MARK_SPREAD_DELAY )
            {
                m_animFrame = ( float( t - m_eventTime ) - MARK_SPREAD_DELAY ) / EXPLODE_FRAME_LEN;
                spread_mark();
            }
            else
                m_animFrame = 0;

            if( t - m_eventTime > MARK_BREAK_DELAY )
            {
                explode();
                //Game::Inst().playSound(bubble_pop_02);
            }
			break;
		}
		case STATE_EXPLODING:
		{
			spread_mark();
            if( t - m_eventTime > MARK_EXPLODE_DELAY )
                die();
			break;
		}
        //clear this out in update, so it doesn't bash moving balls
        case STATE_GONE:
        {
            Int2 vec = { m_col * 4, m_row * 4 };
            m_pWrapper->QueueClear( vec );
            //vid.bg0.image(vec, GemEmpty, 0);
            break;
        }
		default:
			break;
	}
}


void GridSlot::mark()
{
    if( m_state == STATE_MARKED || m_state == STATE_EXPLODING )
        return;
    m_animFrame = 0;
	m_state = STATE_MARKED;
    m_eventTime = SystemTime::now();
    Game::Inst().playSound(match2);
    Game::Inst().SetChain( true );

    if( m_color < NUM_COLORS )
        Game::Inst().SetUsedColor( m_color );
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

    hurtNeighboringRock( m_row - 1, m_col );
    hurtNeighboringRock( m_row, m_col - 1 );
    hurtNeighboringRock( m_row + 1, m_col );
    hurtNeighboringRock( m_row, m_col + 1 );

    if( m_multiplier > 1 )
    {
        Game::Inst().UpMultiplier();
        m_multiplier = 1;
    }

	m_eventTime = SystemTime::now();
}

void GridSlot::die()
{
    m_state = STATE_GONE;
    m_bFixed = false;
	m_score = Game::Inst().getIncrementScore();
    Game::Inst().CheckChain( m_pWrapper, vec( m_row, m_col ) );
    m_pWrapper->checkEmpty();
    m_eventTime = SystemTime::now();
}


void GridSlot::markNeighbor( int row, int col )
{
	//find my neighbor and see if we match
	GridSlot *pNeighbor = m_pWrapper->GetSlot( row, col );

	//PRINT( "pneighbor = %p", pNeighbor );
	//PRINT( "color = %d", pNeighbor->getColor() );
    if( pNeighbor && pNeighbor->isMatchable() && !pNeighbor->isMarked() && pNeighbor->getColor() == m_color )
		pNeighbor->mark();
}


void GridSlot::hurtNeighboringRock( int row, int col )
{
    //find my neighbor and see if we match
    GridSlot *pNeighbor = m_pWrapper->GetSlot( row, col );

    //PRINT( "pneighbor = %p", pNeighbor );
    //PRINT( "color = %d", pNeighbor->getColor() );
    if( pNeighbor && pNeighbor->getColor() == ROCKCOLOR )
        pNeighbor->DamageRock();
}


void GridSlot::DamageRock()
{
    if( m_RockHealth > 0 )
    {
        Int2 vec = { m_col * 4, m_row * 4 };

        m_RockHealth--;

        m_pWrapper->SpawnRockExplosion( vec, m_RockHealth );

        if( m_RockHealth == 0 )
            explode();
    }
}



//copy color and some other attributes from target.  Used when tilting
void GridSlot::TiltFrom(GridSlot &src)
{
    FillColor( src.m_color );
    m_Movestate = MOVESTATE_PENDINGMOVE;
    m_state = src.m_state;
	m_eventTime = src.m_eventTime;
	m_curMovePos.x = src.m_col * 4;
	m_curMovePos.y = src.m_row * 4;
    m_RockHealth = src.m_RockHealth;
}


//if we have a move pending, start it
void GridSlot::startPendingMove()
{
    if( m_Movestate == MOVESTATE_PENDINGMOVE )
	{
        Game::Inst().playSound(slide_39);
        m_Movestate = MOVESTATE_MOVING;
		m_animFrame = 0;
	}
}


//given tilt state, return our desired frame
unsigned int GridSlot::GetTiltFrame( Float2 &tiltState, Int2 &quantized ) const
{
    //quantize and convert to the appropriate range
    //non-linear quantization.
    quantized = vec( QuantizeTiltValue( tiltState.x ), QuantizeTiltValue( tiltState.y ) );

    return TILTTOFRAMES[ quantized.y ][ quantized.x ];
}



//convert from [-128, 128] to [0, 6] via non-linear quantization
unsigned int GridSlot::QuantizeTiltValue( float value ) const
{
    /*int TILT_THRESHOLD_VALUES[NUM_QUANTIZED_TILT_VALUES] =
    {
        -50,
        -30,
        -10,
        10,
        30,
        50,
        500
    };*/
    int TILT_THRESHOLD_VALUES[NUM_QUANTIZED_TILT_VALUES] =
        {
            -30,
            -20,
            -5,
            5,
            20,
            30,
            500
        };

    for( unsigned int i = 0; i < NUM_QUANTIZED_TILT_VALUES; i++ )
    {
        if( value < TILT_THRESHOLD_VALUES[i] )
            return i;
    }

    return 3;
}


static unsigned int ROLLING_FRAMES[ GridSlot::NUM_ROLL_FRAMES / GridSlot::NUM_FRAMES_PER_ROLL_ANIM_FRAME ] =
{ 1, 3, 23, 20, 17, 14, 11, 8, 10, 13, 17, 16, 0, 4, 0, 16 };

//get the rolling frame of the given index
unsigned int GridSlot::GetRollingFrame( unsigned int index )
{
    ASSERT( index < NUM_ROLL_FRAMES);

    return ROLLING_FRAMES[ index / NUM_FRAMES_PER_ROLL_ANIM_FRAME ];
}


unsigned int GridSlot::GetFixedFrame( unsigned int index )
{
    ASSERT( index / NUM_FRAMES_PER_FIXED_ANIM_FRAME < NUM_FIXED_FRAMES);

    int frame = NUM_FIXED_FRAMES * m_pWrapper->getLastTiltDir() + ( index / NUM_FRAMES_PER_FIXED_ANIM_FRAME ) + 1;

    return frame;
}

/*
static unsigned int IDLE_FRAMES[ GridSlot::NUM_IDLE_FRAMES / GridSlot::NUM_FRAMES_PER_IDLE_ANIM_FRAME ] =
{ FRAME_N1, FRAME_E1, FRAME_S1, FRAME_W1 };


unsigned int GridSlot::GetIdleFrame()
{
    ASSERT( m_pWrapper->IsIdle() );
    if( m_animFrame >= NUM_IDLE_FRAMES )
    {
        m_animFrame = 0;
    }

    return IDLE_FRAMES[ m_animFrame / NUM_FRAMES_PER_IDLE_ANIM_FRAME ];
}
*/


void GridSlot::DrawIntroFrame( VideoBuffer &vid, unsigned int frame )
{
    Int2 vec = { m_col * 4, m_row * 4 };

    if( !isAlive() )
        vid.bg0.image(vec, GemEmpty, 0);
    else if( IsSpecial() )
        vid.bg0.image(vec, GetSpecialTexture(), GetSpecialFrame());
    else
    {
        switch( frame )
        {
            case 0:
            {
                vid.bg0.image(vec, GemEmpty, 0);
                break;
            }
            case 1:
            {
                const FlatAssetImage &exTex = GetExplodingTexture();
                vid.bg0.image(vec, exTex, 1);
                break;
            }
            case 2:
            {
                const FlatAssetImage &exTex = GetExplodingTexture();
                vid.bg0.image(vec, exTex, 0);
                break;
            }
            default:
            {
                const FlatAssetImage &tex = *TEXTURES[ m_color ];
                vid.bg0.image(vec, tex, 0);
                break;
            }
        }
    }
}


void GridSlot::setFixedAttempt()
{
    m_Movestate = MOVESTATE_FIXEDATTEMPT;
    m_animFrame = 0;
    Game::Inst().playSound(frozen_06);
}



void GridSlot::UpMultiplier()
{
    if( isAlive() && IsFixed() && m_multiplier > 1 )
        m_multiplier++;
}


//morph from rainball to given color
void GridSlot::RainballMorph( unsigned int color )
{
    FillColor( color );
    m_bWasRainball = true;
}



//bubble is bumping this chromit, tilt it in the given direction
void GridSlot::Bump( const Float2 &dir )
{
    if( isAlive() && !IsFixed() && !IsSpecial() && ( m_Movestate == MOVESTATE_STATIONARY || m_Movestate == MOVESTATE_BUMPED || m_Movestate == MOVESTATE_FINISHINGMOVE ) )
    {
        m_Movestate = MOVESTATE_BUMPED;

        Int2 newDir = m_lastFrameDir;

        //(2/sqrtf(5))
        const float DIR_THRESH = -.4472136f;

        //push one frame over in dir direction
        if( dir.x < DIR_THRESH )
        {
            newDir.x-=4;
            if( newDir.x < 0 )
                newDir.x = 0;
        }
        else if( dir.x > DIR_THRESH )
        {
            newDir.x+=4;
            if( newDir.x >= (int)NUM_QUANTIZED_TILT_VALUES )
                newDir.x = NUM_QUANTIZED_TILT_VALUES - 1;
        }

        if( dir.y < DIR_THRESH )
        {
            newDir.y-=4;
            if( newDir.y < 0 )
                newDir.y = 0;
        }
        else if( dir.y > DIR_THRESH )
        {
            newDir.y+=4;
            if( newDir.y >= (int)NUM_QUANTIZED_TILT_VALUES )
                newDir.y = NUM_QUANTIZED_TILT_VALUES - 1;
        }

        m_animFrame = TILTTOFRAMES[ newDir.y ][ newDir.x ];
        m_lastFrameDir = newDir;
    }
}
