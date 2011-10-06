/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sifteo.h>
#include "assets.gen.h"
#include "cubewrapper.h"

#define PRINTS_ON 0
#if PRINTS_ON
#define PRINT(...) printf (__VA_ARGS__)
#else
#define PRINT(...) 
#endif

using namespace Sifteo;

/*static const int NUM_CUBES = 2;

static CubeWrapper cubes[NUM_CUBES] = 
{
	CubeWrapper((_SYSCubeID)0),
	CubeWrapper((_SYSCubeID)1),
};*/
static const int NUM_CUBES = 1;

static CubeWrapper cubes[NUM_CUBES] = 
{
	CubeWrapper((_SYSCubeID)0),
};


static void onAccelChange(_SYSCubeID cid)
{
    _SYSAccelState state;
    _SYS_getAccel(cid, &state);
}

static void init()
{
	for( int i = 0; i < NUM_CUBES; i++ )
		cubes[i].Init(GameAssets);

	bool done = false;

	PRINT( "getting ready to load" );

	while( !done )
	{
		done = true;
		for( int i = 0; i < NUM_CUBES; i++ )
		{
			if( !cubes[i].DrawProgress(GameAssets) )
				done = false;

			PRINT( "in load loop" );
		}
		System::paint();
	}
	PRINT( "done loading" );
	for( int i = 0; i < NUM_CUBES; i++ )
		cubes[i].vidInit();
}

void siftmain()
{
    init();

    /*vid.BG0_text(Vec2(2,1), Font, "Hello World!");
	vid.BG0_textf(Vec2(2,6), Font, "Time: %4u.%u", (int)t, (int)(t*10) % 10);
    vid.BG0_drawAsset(Vec2(1,10), Logo);
 */
    _SYS_vectors.accelChange = onAccelChange;

	for( int i = 0; i < NUM_CUBES; i++ )
		onAccelChange(i);

    while (1) {
        //float t = System::clock();
        
        for( int i = 0; i < NUM_CUBES; i++ )
			cubes[i].Draw();
            
        System::paint();
    }
}
