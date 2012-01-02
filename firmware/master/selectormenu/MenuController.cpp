/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "MenuController.h"
#include "TFutils.h"
#include "assets.gen.h"

namespace SelectorMenu
{

TiltFlowItem MENUITEMS[ MenuController::NUM_MENU_ITEMS ] =
{
    TiltFlowItem( IconGameChroma, LabelChroma ),
    TiltFlowItem( IconGameWord, LabelWord ),
    TiltFlowItem( IconGameSandwich, LabelSandwich ),
    //TiltFlowItem( IconGamePeano, LabelPeano ),
    TiltFlowItem( IconGameMore, LabelMore ),
};

MenuController &s_menu = MenuController::Inst();

/*
static void onAccelChange(_SYSCubeID cid)
{
    _SYSAccelState state;
    _SYS_getAccel(cid, &state);

    static const int TILT_THRESHOLD = 20;

    menu.cubes[cid].ClearTiltInfo();

    if( state.x > TILT_THRESHOLD )
        menu.cubes[cid].AddTiltInfo( RIGHT );
    else if( state.x < -TILT_THRESHOLD )
        menu.cubes[cid].AddTiltInfo( LEFT );
    if( state.y > TILT_THRESHOLD )
        menu.cubes[cid].AddTiltInfo( DOWN );
    else if( state.y < -TILT_THRESHOLD )
        menu.cubes[cid].AddTiltInfo( UP);
}
*/

static void onTilt(_SYSCubeID cid)
{
    Cube::TiltState state = s_menu.cubes[cid].GetCube().getTiltState();

    if( state.x == _SYS_TILT_POSITIVE )
        s_menu.cubes[cid].Tilt( RIGHT );
    else if( state.x == _SYS_TILT_NEGATIVE )
        s_menu.cubes[cid].Tilt( LEFT );
    if( state.y == _SYS_TILT_POSITIVE )
        s_menu.cubes[cid].Tilt( DOWN );
    else if( state.y == _SYS_TILT_NEGATIVE )
        s_menu.cubes[cid].Tilt( UP);
}

static void onShake(_SYSCubeID cid)
{
    _SYS_ShakeState state;
    _SYS_getShake(cid, &state);
    s_menu.cubes[cid].Shake(state);
}


void RunMenu()
{
    s_menu.Init();

    _SYS_vectors.cubeEvents.tilt = onTilt;
    _SYS_vectors.cubeEvents.shake = onShake;

    while (s_menu.Update()) {}
}


MenuController &MenuController::Inst()
{
    static MenuController menu = MenuController();

    return menu;
}

MenuController::MenuController() : m_Menu( MENUITEMS, NUM_MENU_ITEMS, NUM_CUBES )
{
	//Reset();
}


void MenuController::Init()
{
	for( int i = 0; i < NUM_CUBES; i++ )
        cubes[i].Init(MenuControllerAssets);

    m_Menu.AssignViews();

	bool done = false;

	PRINT( "getting ready to load" );

	while( !done )
	{
		done = true;
		for( int i = 0; i < NUM_CUBES; i++ )
		{
            if( !cubes[i].DrawProgress(MenuControllerAssets) )
				done = false;

			PRINT( "in load loop" );
		}
		System::paint();
	}
	PRINT( "done loading" );
	for( int i = 0; i < NUM_CUBES; i++ )
		cubes[i].vidInit();

    m_fLastTime = System::clock();
}


bool MenuController::Update()
{
    float t = System::clock();
    float dt = t - m_fLastTime;
    m_fLastTime = t;

    bool result = m_Menu.Tick(dt);
            
    System::paint();

	return result;
}

void MenuController::Reset()
{
	for( int i = 0; i < NUM_CUBES; i++ )
	{
		cubes[i].Reset();
	}
}


} //namespace SelectorMenu
