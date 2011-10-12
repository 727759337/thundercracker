/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "cubewrapper.h"
#include "game.h"
#include "assets.gen.h"
#include "utils.h"

static _SYSCubeID s_id = 0;

static Cube *s_pCube = NULL;

CubeWrapper::CubeWrapper() : m_cube(s_id++), m_vid(m_cube.vbuf), m_rom(m_cube.vbuf), m_frame(0)
{
	for( int i = 0; i < NUM_SIDES; i++ )
	{
		m_neighbors[i] = false;
	}

	for( int i = 0; i < NUM_ROWS; i++ )
	{
		for( int j = 0; j < NUM_COLS; j++ )
		{
			GridSlot &slot = m_grid[i][j];
			slot.Init( this, i, j );
		}
	}

	if( s_id == 1 )
	{
		s_pCube = &m_cube;
	}
}


void CubeWrapper::Init( AssetGroup &assets )
{
	m_cube.enable();
    m_cube.loadAssets( assets );

    m_rom.init();
    m_rom.BG0_text(Vec2(1,1), "Loading...");
}

void CubeWrapper::Disable()
{
	m_cube.disable();
}

bool CubeWrapper::DrawProgress( AssetGroup &assets )
{
	m_rom.BG0_progressBar(Vec2(0,7), m_cube.assetProgress(GameAssets, m_vid.LCD_width), 2);
        
	return m_cube.assetDone(assets);
}

void CubeWrapper::Draw()
{
	//draw grid
	for( int i = 0; i < NUM_ROWS; i++ )
	{
		for( int j = 0; j < NUM_COLS; j++ )
		{
			GridSlot &slot = m_grid[i][j];
			slot.Draw( m_vid, Vec2(j * 4, i * 4) );
		}
	}
}


void CubeWrapper::DrawSplash()
{
	m_vid.BG0_drawAsset(Vec2(0,0), Cover, 0);
}


void CubeWrapper::Update(float t)
{
	uint8_t buf[4];
    _SYS_getRawNeighbors(m_cube.id(), buf);

	for( int i = 0; i < NUM_SIDES; i++ )
	{
		//TODO, this should be pushed into sdk
		//also, it doesn't check what cubes are its neighbors
		bool newValue = ( buf[i] >> 7 ) > 0;

		//newly neighbored
		if( newValue && !m_neighbors[i])
			Game::Inst().setTestMatchFlag();
		m_neighbors[i] = newValue;
	}

	//update all dots
	for( int i = 0; i < NUM_ROWS; i++ )
	{
		for( int j = 0; j < NUM_COLS; j++ )
		{
			GridSlot &slot = m_grid[i][j];
			slot.Update( t );
		}
	}
}


void CubeWrapper::vidInit()
{
	m_vid.init();
}


void CubeWrapper::Tilt( int dir )
{
	bool bChanged = false;

	PRINT( "tilting" );

	//hastily ported from the python
	switch( dir )
	{
		case 0:
		{
			for( int i = 0; i < NUM_COLS; i++ )
			{
				for( int j = NUM_ROWS - 1; j >= 0; j-- )
				{
					//start shifting it over
					for( int k = j - 1; k >= 0; k-- )
					{
						if( TryMove( j, i, k, i ) )
							bChanged = true;
						else
							break;
					}
				}
			}
			break;
		}
		case 1:
		{
			for( int i = 0; i < NUM_ROWS; i++ )
			{
				for( int j = NUM_COLS - 1; j >= 0; j-- )
				{
					//start shifting it over
					for( int k = j - 1; k >= 0; k-- )
					{
						if( TryMove( i, j, i, k ) )
							bChanged = true;
						else
							break;
					}
				}
			}
			break;
		}
		case 2:
		{
			for( int i = 0; i < NUM_COLS; i++ )
			{
				for( int j = 0; j < NUM_ROWS; j++ )
				{
					//start shifting it over
					for( int k = j + 1; k < NUM_ROWS; k++ )
					{
						if( TryMove( j, i, k, i ) )
							bChanged = true;
						else
							break;
					}
				}
			}
			break;
		}
		case 3:
		{
			for( int i = 0; i < NUM_ROWS; i++ )
			{
				for( int j = 0; j < NUM_COLS; j++ )	
				{
					//start shifting it over
					for( int k = j + 1; k < NUM_COLS; k++ )
					{
						if( TryMove( i, j, i, k ) )
							bChanged = true;
						else
							break;
					}
				}
			}
			break;
		}
	}        

	if( bChanged )
		Game::Inst().setTestMatchFlag();
}

//try moving a gem from row1/col1 to row2/col2
//return if successful
bool CubeWrapper::TryMove( int row1, int col1, int row2, int col2 )
{
	//start shifting it over
	GridSlot &slot = m_grid[row1][col1];
	GridSlot &dest = m_grid[row2][col2];

	if( !dest.isEmpty() )
		return false;

	if( slot.isAlive() )
	{
		dest = slot;
		slot.setEmpty();
		return true;
	}

	return false;
}


//only test matches with neighbors with id less than ours.  This prevents double testing
void CubeWrapper::testMatches()
{
	//TODO super hacky for the demo, since we don't know which cube is our neighbor
	if( m_cube.id() < 1 )
		return;

	for( int i = 0; i < NUM_SIDES; i++ )
	{
		if( m_neighbors[i] /*&& m_cube.neighbors[i].id() < m_cube.id()*/ )
		{
			//TEMP try marking everything
			/*for( int k = 0; k < NUM_ROWS; k++ )
			{
				for( int l = 0; l < NUM_ROWS; l++ )
				{
					if( m_grid[k][l].isAlive() )
						m_grid[k][l].mark();
				}
			}

			return;*/

			//as long we we test one block going clockwise, and the other going counter-clockwise, we'll match up
			//int side = GetSideNeighboredOn( m_cube.neighbors[i], m_cube );
			//TODO fix this so it actually knows which cube it's looking at
			int side = Game::Inst().cubes[0].GetSideNeighboredOn( 0, m_cube );

			//no good, just set our flag again and return
			if( side < 0 )
			{
				Game::Inst().setTestMatchFlag();
				return;
			}

			//fill two 4 element pointer arrays of grid slots representing what we need to match up
			GridSlot *ourGems[4];
			GridSlot *theirGems[4];

			FillSlotArray( ourGems, i, true );

			//TODO total hackery
			CubeWrapper &otherCube = Game::Inst().cubes[0];
			otherCube.FillSlotArray( theirGems, side, false );

			//compare the two
			for( int j = 0; j < NUM_ROWS; j++ )
			{
				if( ourGems[j]->isAlive() && theirGems[j]->isAlive() && ourGems[j]->getColor() == theirGems[j]->getColor() )
				{
					ourGems[j]->mark();
					theirGems[j]->mark();
				}
			}
		}
	}
}


void CubeWrapper::FillSlotArray( GridSlot **gems, int side, bool clockwise )
{
	//TODO for now, go with the SDK's top right bottom left order.  fix later
	switch( side )
	{
		case 0:
		{
			if( clockwise )
			{
				for( int i = 0; i < NUM_COLS; i++ )
					gems[i] = &m_grid[0][i];
			}
			else
			{
				for( int i = 0; i < NUM_COLS; i++ )
					gems[NUM_COLS - i - 1] = &m_grid[0][i];
			}
			break;
		}
		case 1:
		{
			if( clockwise )
			{
				for( int i = 0; i < NUM_ROWS; i++ )
					gems[NUM_ROWS - i - 1] = &m_grid[i][0];
			}
			else
			{
				for( int i = 0; i < NUM_ROWS; i++ )
					gems[i] = &m_grid[i][0];
			}
			break;
		}
		case 2:
		{
			if( clockwise )
			{
				for( int i = 0; i < NUM_COLS; i++ )
					gems[NUM_COLS - i - 1] = &m_grid[NUM_ROWS - 1][i];
			}
			else
			{
				for( int i = 0; i < NUM_COLS; i++ )
					gems[i] = &m_grid[NUM_ROWS - 1][i];
			}
			break;
		}
		case 3:
		{
			if( clockwise )
			{
				for( int i = 0; i < NUM_ROWS; i++ )
					gems[i] = &m_grid[i][NUM_COLS - 1];
			}
			else
			{
				for( int i = 0; i < NUM_ROWS; i++ )
					gems[NUM_ROWS - i - 1] = &m_grid[i][NUM_COLS - 1];
			}
			break;
		}
		default:
			return;
	}

	//debug
	PRINT( "side %d, clockwise %d\n", side, clockwise );
	for( int i = 0; i < NUM_ROWS; i++ )
		PRINT( "gem %d: Alive %d, color %d\n", i, gems[i]->isAlive(), gems[i]->getColor() );
}


//TODO, this is total fakery now.  Not sure what cube are our neighbors
int CubeWrapper::GetSideNeighboredOn( _SYSCubeID id, Cube &cube )
{
	for( int i = 0; i < NUM_SIDES; i++ )
	{
		if( m_neighbors[i] )
			return i;
	}

	return -1;
}


//if all gems are living or gone, nothing is happening
bool CubeWrapper::IsQuiet() const
{
	for( int i = 0; i < NUM_ROWS; i++ )
	{
		for( int j = 0; j < NUM_COLS; j++ )
		{
			const GridSlot &slot = m_grid[i][j];
			if( !(slot.isAlive() || slot.isEmpty() ) )
				return false;
		}
	}

	return true;
}

GridSlot *CubeWrapper::GetSlot( int row, int col )
{
	//PRINT( "trying to retrieve %d, %d", row, col );
	if( row >= 0 && row < NUM_ROWS && col >= 0 && col < NUM_COLS )
	{
		return &(m_grid[row][col]);
	}

	return NULL;
}


//superhacky, include Beth's demo

#include "monsters_small.h"



void solidFrames(uint16_t color, unsigned num)
{
    VidMode_Solid vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    vid.setColor(color);
    vid.set();

    while (num--)
        System::paint();
}

void whiteFlash()
{
    solidFrames(0xFFFF, 5);
}

void panToEarth()
{
    VidMode_BG0 vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    vid.clear(Black.tiles[0]);
    vid.BG0_drawAsset(Vec2(0,0), Globe64);
    vid.set();

    // Negative acceleration simulation, with overshoot

    float x = -10.0f;
    float y = 30.0f;
    float vx = 10.0f;
    float vy = -7.0f;

    const float damping = 0.99f;
    const float gain = 0.03f;

    for (unsigned i = 0; i < 160; i++) {
        x += vx;
        y += vy;
        vx -= gain * x;
        vy -= gain * y;
        vx *= damping;
        vy *= damping;

        vid.BG0_setPanning(Vec2( x - (128/2 - 64/2) + 0.5f,
                                 y - (128/2 - 64/2) + 0.5f ));
        System::paint();
    }
}

void earthZoomSlow()
{
    /*
     * Slow rotate/zoom, from a distance
     */

    VidMode_BG2 vid(s_pCube->vbuf);
    const int height = 100;

    solidFrames(0x0000, 3);
    vid.setWindow((128 - height) / 2, height);
    vid.clear(Black.tiles[0]);
    vid.BG2_drawAsset(Vec2(0,0), Globe128);
    vid.BG2_setBorder(0x0000);
    vid.set();

    for (float t = 0; t < 1.0f; t += 0.03f) {
        AffineMatrix m = AffineMatrix::identity();

        m.translate(64, 64);
        m.scale(0.5f + t * 0.4f);
        m.rotate(t * 0.5f);
        m.translate(-64, -64 + (128 - height) / 2);

        vid.BG2_setMatrix(m);
        System::paint();
    }
}

void lbZoom(const Sifteo::AssetImage &asset, int cx, int cy, float z1, float z2)
{
    /*
     * Fast rotate/zoom, close up and letterboxed
     */

    VidMode_BG2 vid(s_pCube->vbuf);
    const int height = 90;

    solidFrames(0x0000, 3);
    vid.setWindow((128 - height) / 2, height);
    vid.clear(Black.tiles[0]);
    vid.BG2_drawAsset(Vec2(0,0), asset);
    vid.BG2_setBorder(0x0000);
    vid.set();

    for (float t = 0; t < 1.0f; t += 0.015f) {
        AffineMatrix m = AffineMatrix::identity();

        m.translate(cx, cy);
        m.scale(z1 + t * z2);
        m.rotate(t * 0.7f - 1.0f);
        m.translate(-cx, -cy);

        vid.BG2_setMatrix(m);
        System::paint();
    }
}

void showMonsters() {
    VidMode vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    _SYS_vbuf_pokeb(&s_pCube->vbuf.sys, offsetof(_SYSVideoRAM, mode), _SYS_VM_FB32);

    for (unsigned loops = 0; loops < 2; loops++)
        for (unsigned i = 0; i < arraysize(monsters); i++) {
            const MonsterData *m = monsters[i];

            _SYS_vbuf_write(&s_pCube->vbuf.sys, 0, (const uint16_t *) m, 256);
            _SYS_vbuf_write(&s_pCube->vbuf.sys, 384, 256 + (const uint16_t *) m, 16);

            for (unsigned j = 0; j < 3; j++)
                System::paint();
        }
}

void owlbearRun()
{
    VidMode_BG0 vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    vid.clear(Black.tiles[0]);
    vid.BG0_setPanning(Vec2(0,0));
    vid.set();

    for (unsigned loops = 0; loops < 20; loops++)
        for (unsigned frame = 0; frame < Owlbear.frames; frame++) {
            vid.BG0_drawAsset(Vec2(0,0), Owlbear, frame);
            System::paint();
        }
}

void brickScroll()
{
    VidMode_BG0 vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    vid.BG0_drawAsset(Vec2(0,0), Brick144);
    vid.set();
    
    for (unsigned frame = 0; frame < 600; frame++) {

        if (frame == 300) {
            _SYS_vbuf_pokeb(&s_pCube->vbuf.sys, offsetof(_SYSVideoRAM, mode), _SYS_VM_BG0_BG1);
            _SYS_vbuf_fill(&s_pCube->vbuf.sys, offsetof(_SYSVideoRAM, bg1_bitmap) / 2 + 3, 0x0FF0, 9);
            _SYS_vbuf_writei(&s_pCube->vbuf.sys, offsetof(_SYSVideoRAM, bg1_tiles) / 2, Cat.tiles, 0, 8*9);
        }

        vid.BG0_setPanning(Vec2( frame * 2.5f + 0.5f, sinf(frame * 0.06f) * 60.0f + 0.5f ));
        System::paint();
    }
}


void randomGems()
{
    VidMode_BG0 vid(s_pCube->vbuf);

    vid.setWindow(0, 128);
    vid.BG0_setPanning(Vec2(0,0));

    for (unsigned frames = 0; frames < 260; frames++) {
        for (unsigned x = 0; x < 16; x += 2)
            for (unsigned y = 0; y < 16; y += 2)
                vid.BG0_drawAsset(Vec2(x,y), Gems, Game::Inst().Rand(Gems.frames));

        vid.set();
        System::paint();
    }
}


void CubeWrapper::HelloInit()
{
	for( int i = 0; i < 16; i++ )
	{
		for( int j = 0; j < 16; j++ )
		{
			m_vid.BG0_drawAsset( Vec2(i,j), White );
		}
	}
	//m_vid.clear(White.tiles[0]);
	m_vid.BG0_text(Vec2(2,1), WhiteFont, "Hello World!");
    m_vid.BG0_drawAsset(Vec2(1,10), Logo);
}


void CubeWrapper::Hello()
{
    const unsigned rate = 2;

    float t = System::clock();

    m_vid.BG0_textf(Vec2(2,6), WhiteFont, "Time: %4u.%u", (int)t, (int)(t*10) % 10);
    m_vid.BG0_drawAsset(Vec2(11,9), Kirby, m_frame >> rate);

    if (++m_frame == Kirby.frames << rate)
        m_frame = 0;

	uint8_t buf[4];
	_SYS_getRawNeighbors(m_cube.id(), buf);

	for( int i = 0; i < NUM_SIDES; i++ )
	{
		if( ( buf[i] >> 7 ) > 0 )
			Game::Inst().setState( Game::STATE_GFXDEMO );
	}
}


void CubeWrapper::HelloTilt(_SYSAccelState &state)
{
	m_vid.BG0_textf(Vec2(2,4), WhiteFont, "Tilt: %02x %02x", state.x + 0x80, state.y + 0x80);
	m_vid.BG0_setPanning(Vec2(-state.x/2, -state.y/2));
}

void CubeWrapper::Demo()
{
	while (1) {
        whiteFlash();
        panToEarth();

        whiteFlash();
        earthZoomSlow();

        whiteFlash();
        lbZoom(Globe128, 30, 20, 0.5f, 2.5f);

        whiteFlash();
        lbZoom(Clouds, 74, 44, 0.5f, 2.0f);

        whiteFlash();
        lbZoom(Flower, 74, 28, 0.5f, 3.0f);

        whiteFlash();
        showMonsters();

        whiteFlash();
        owlbearRun();

        whiteFlash();
        randomGems();

        whiteFlash();
        brickScroll();
    }
}
