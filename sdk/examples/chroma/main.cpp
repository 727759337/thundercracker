/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <sifteo.h>
#include "assets.gen.h"
#include "cubewrapper.h"
#include "game.h"
#include "utils.h"

using namespace Sifteo;

static Game &game = Game::Inst();

/*
static void onAccelChange(_SYSCubeID cid)
{
    _SYSAccelState state;
    _SYS_getAccel(cid, &state);

	static const int TILT_THRESHOLD = 20;

	game.cubes[cid].ClearTiltInfo();

	if( state.x > TILT_THRESHOLD )
		game.cubes[cid].AddTiltInfo( RIGHT );
	else if( state.x < -TILT_THRESHOLD )
		game.cubes[cid].AddTiltInfo( LEFT );
	if( state.y > TILT_THRESHOLD )
		game.cubes[cid].AddTiltInfo( DOWN );
	else if( state.y < -TILT_THRESHOLD )
		game.cubes[cid].AddTiltInfo( UP);
}
*/

static void onTilt(_SYSCubeID cid)
{
    Cube::TiltState state = game.cubes[cid - CUBE_ID_BASE].GetCube().getTiltState();

	if( state.x == _SYS_TILT_POSITIVE )
        game.cubes[cid - CUBE_ID_BASE].Tilt( RIGHT );
	else if( state.x == _SYS_TILT_NEGATIVE )
        game.cubes[cid - CUBE_ID_BASE].Tilt( LEFT );
	if( state.y == _SYS_TILT_POSITIVE )
        game.cubes[cid - CUBE_ID_BASE].Tilt( DOWN );
	else if( state.y == _SYS_TILT_NEGATIVE )
        game.cubes[cid - CUBE_ID_BASE].Tilt( UP);
}

static void onShake(_SYSCubeID cid)
{
    _SYSShakeState state;
    _SYS_getShake(cid, &state);
    game.cubes[cid - CUBE_ID_BASE].Shake(state);
}

static void init()
{
	game.Init();
}

void siftmain()
{
    init();

    //_SYS_vectors.cubeEvents.accelChange = onAccelChange;
    _SYS_vectors.cubeEvents.tilt = onTilt;
	_SYS_vectors.cubeEvents.shake = onShake;
    _SYS_vectors.neighborEvents.add = 0;

    while (1) {
        game.Update();        
    }
}
