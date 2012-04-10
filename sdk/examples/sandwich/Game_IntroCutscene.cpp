#include "Game.h"

Cube* Game::IntroCutscene() {
	for(unsigned i=0; i<NUM_CUBES; ++i) {
		ViewMode gfx(gCubes[i].vbuf);
		gfx.set();
		gfx.BG0_drawAsset(vec(0,0), Sting);
		for(unsigned s=0; s<8; ++s) {
			gfx.hideSprite(s);
		}
		BG1Helper overlay(gCubes[i]);
		overlay.DrawAsset(vec(0,0), SandwichTitle);
		overlay.Flush();
		gfx.BG1_setPanning(vec(0,64));
		gCubes[i].vbuf.touch();
	}
	float dt;
	for(SystemTime t=SystemTime::now(); (dt=(SystemTime::now()-t))<0.9f;) {
		float u = 1.f - (dt / 0.9f);
		u = 1.f - (u*u*u*u);
		for(unsigned i=0; i<NUM_CUBES; ++i) {
			ViewMode(gCubes[i].vbuf).BG1_setPanning(vec(0.f, 72 - 128*u));
		}
		DoPaint(true);
	}
	
	// wait for a touch
	Cube* pCube = 0;
	while(!pCube) {
		for(unsigned i=0; i<NUM_CUBES; ++i) {
			if (gCubes[i].touching()) {
				pCube = gCubes + i;
				PlaySfx(sfx_neighbor);
				break;
			}
		}
		System::yield();
	}
	// blank other cubes
	for(unsigned i=0; i<NUM_CUBES; ++i) {
		if (gCubes+i != pCube) {
			BG1Helper(gCubes[i]).Flush();
			ViewMode gfx(gCubes[i].vbuf);
			gfx.BG1_setPanning(vec(0,0));
			gfx.BG0_drawAsset(vec(0,0), Blank);
		}
	}
	ViewMode mode(pCube->vbuf);
	// hide banner
	for(SystemTime t=SystemTime::now(); (dt=(SystemTime::now()-t))<0.9f;) {
		float u = 1.f - (dt / 0.9f);
		u = 1.f - (u*u*u*u);
		mode.BG1_setPanning(Vec2(0.f, -56 + 128*u));
		DoPaint(true);
	}
	WaitForSeconds(0.1f);
	// pearl walks up from bottom
	PlaySfx(sfx_running);
	int framesPerCycle = PlayerWalk.frames >> 2;
	int tilesPerFrame = PlayerWalk.width * PlayerWalk.height;
	mode.resizeSprite(0, 32, 32);
	for(unsigned i=0; i<48/2; ++i) {
		mode.setSpriteImage(0, PlayerWalk.index + tilesPerFrame * (i%framesPerCycle));
		mode.moveSprite(0, 64-16, 128-i-i);
		DoPaint(true);
	}
	// face front
	mode.setSpriteImage(0, PlayerStand.index + SIDE_BOTTOM * (PlayerStand.width * PlayerStand.height));
	WaitForSeconds(0.5f);

	// look left
	mode.setSpriteImage(0, PlayerIdle.index);
	WaitForSeconds(0.5f);
	mode.setSpriteImage(0, PlayerStand.index + SIDE_BOTTOM * (PlayerStand.width * PlayerStand.height));
	WaitForSeconds(0.5f);

	// look right
	mode.setSpriteImage(0, PlayerIdle.index + (PlayerIdle.width * PlayerIdle.height));
	WaitForSeconds(0.5f);
	mode.setSpriteImage(0, PlayerStand.index + SIDE_BOTTOM * (PlayerStand.width * PlayerStand.height));
	WaitForSeconds(0.5f);

	// thought bubble appears
	mode.BG0_drawAsset(vec(10,8), TitleThoughts);
	WaitForSeconds(0.5f);
	mode.BG0_drawAsset(vec(3,4), TitleBalloon);
	WaitForSeconds(0.5f);

	// items appear
	const int pad = 36;
	const int innerPad = (128 - pad - pad) / 3;
	for(unsigned i = 0; i < 4; ++i) {
		int x = pad + innerPad * i - 8;
		mode.setSpriteImage(i+1, Items, i);
		mode.resizeSprite(i+1, 16, 16);
		// jump
		//PlaySfx(sfx_pickup);
		for(int j=0; j<6; j++) {
			mode.moveSprite(i+1, x, 42 - j);
			DoPaint(false);
		}
		for(int j=6; j>0; --j) {
			mode.moveSprite(i+1, x, 42 - j);
			DoPaint(false);
		}
		mode.moveSprite(i+1, x, 42);
		DoPaint(true);
	}
	WaitForSeconds(1.f);

	// do the pickup animation
	for(unsigned i=0; i<PlayerPickup.frames; ++i) {
		mode.setSpriteImage(0, PlayerPickup.index + i * PlayerPickup.width * PlayerPickup.height);
		DoPaint(true);
		WaitForSeconds(0.05f);
	}
	mode.setSpriteImage(0, PlayerStand.index + SIDE_BOTTOM * (PlayerStand.width * PlayerStand.height));
	WaitForSeconds(2.f);

	// hide items and bubble
	mode.hideSprite(1);
	mode.hideSprite(2);
	mode.hideSprite(3);
	mode.hideSprite(4);
<<<<<<< HEAD
	mode.BG0_drawAsset(Vec2(0,0), Sting);
	DoPaint(true);
=======
	mode.BG0_drawAsset(vec(0,0), Sting);
	System::paintSync();
>>>>>>> dd11450dde0d4a58ceaa0ca1600ad57df25d12d8

	// walk off
	PlaySfx(sfx_running);
	unsigned downIndex = PlayerWalk.index + SIDE_BOTTOM * tilesPerFrame * framesPerCycle;
	for(unsigned i=0; i<76/2; ++i) {
		mode.setSpriteImage(0, PlayerWalk.index + tilesPerFrame * (i%framesPerCycle));
		mode.moveSprite(0, 64-16, 80-i-i);
		DoPaint(true);
	}
	mode.hideSprite(0);

	// iris out
	for(unsigned i=0; i<8; ++i) {
		for(unsigned x=i; x<16-i; ++x) {
			mode.BG0_putTile(vec(x, i), *BlackTile.tiles);
			mode.BG0_putTile(vec(x, 16-i-1), *BlackTile.tiles);
		}
		for(unsigned y=i+1; y<16-i-1; ++y) {
			mode.BG0_putTile(vec(i, y), *BlackTile.tiles);
			mode.BG0_putTile(vec(16-i-1, y), *BlackTile.tiles);
		}
		DoPaint(true);
	}
	BG1Helper(*pCube).Flush();
	ViewMode(pCube->vbuf).BG1_setPanning(vec(0,0));
	WaitForSeconds(0.5f);
	return pCube;
}
