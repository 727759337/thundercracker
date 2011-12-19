#include "Game.h"

static Game sGame;
Game* pGame = &sGame;

void WinScreenTest();

void siftmain() {
	// TODO: Move Cubes out of GameView to separate Alloc?
	{ // initialize assets
	  for (Cube::ID i = 0; i < NUM_CUBES; i++) {
	    sGame.CubeAt(i)->enable(i);
	    sGame.CubeAt(i)->loadAssets(GameAssets);
	    VidMode_BG0_ROM rom(sGame.CubeAt(i)->vbuf);
	    rom.init();
	    rom.BG0_text(Vec2(1,1), "Loading...");
	  }
	  bool done = false;
	  while(!done) {
	  	done = true;
	    for (Cube::ID i = 0; i < NUM_CUBES; i++) {
	      VidMode_BG0_ROM rom(sGame.CubeAt(i)->vbuf);
	      rom.BG0_progressBar(Vec2(0,7), sGame.CubeAt(i)->assetProgress(GameAssets, VidMode_BG0::LCD_width), 2);
	      done &= sGame.CubeAt(i)->assetDone(GameAssets);
	    }
	    System::paint();
	  }
	  for(Cube::ID i=0; i<NUM_CUBES; ++i) {
	    VidMode_BG0 mode(sGame.CubeAt(i)->vbuf);
	    mode.init();
	    mode.BG0_drawAsset(Vec2(0,0), Sting);
	  }
	  float time = System::clock();
	  while(System::clock() - time < 1.f) {
	  	for(int i=0; i<NUM_CUBES; ++i) {
	  		sGame.CubeAt(i)->vbuf.touch();
	  	}
	    System::paint();
	  }		
	}

	WinScreenTest();

	sGame.MainLoop();
}

struct Spartikle {
	unsigned frame;
	float px;
	float py;
	float vx;
	float vy;

	inline void Update(float dt, GameView* v, int id) {
		vy += dt * 9.8f;
		px += dt * vx;
		py += dt * vy;
		if (py > 134.0f) {
			py = 268.0f - py;
			vy *= -0.5f;
		}
		frame = (frame+1)%4;
		v->SetSpriteImage(id, Sparkle.index + frame);
		v->MoveSprite(id, (int)(px+0.5f)-4, (int)(py+0.5f)-4);
	}

	inline void SetVel(float x, float y) {
		vx = x;
		vy = y;
	}
};

void WinScreenTest() {
	sGame.ViewAt(0)->EnterSpriteMode();
	for(;;) {
		Cube& cube = *(sGame.CubeAt(0));
		{
			VidMode_BG0 mode(cube.vbuf);
	    	mode.BG0_drawAsset(Vec2(0,0), WinscreenBackground);
	    }
	    for(unsigned frame=0; frame<WinscreenAnim.frames; ++frame) {
	  	    BG1Helper mode(cube);
	  	    mode.DrawAsset(Vec2(9, 0), WinscreenAnim, frame);
	  	    mode.Flush();
	    	System::paintSync();
	    }
		{
			VidMode_BG0 mode(cube.vbuf);
			for(unsigned row=0; row<8; ++row) {
				for(unsigned col=0; col<16; ++col) {
					mode.BG0_drawAsset(Vec2(col,8+row), Flash);
					mode.BG0_drawAsset(Vec2(col,8-row-1), Flash);
				}
				if (row%2 == 1) {
					System::paintSync();
				}
			}
			for(unsigned row=0; row<8; ++row) {
				unsigned top = 7-row;
				unsigned height = 2 * (row+1);
				mode.BG0_drawPartialAsset(Vec2(0, top), Vec2(0, top), Vec2(16, height), Winscreen);
				if (row%2 == 1) {
					System::paintSync();
				}
			}
	    }
	    {
	    	BG1Helper mode(cube);
	    	mode.Flush();
	    }
	    {
	    	VidMode_BG0 mode(cube.vbuf);
	    	mode.BG0_drawAsset(Vec2(0,0), Winscreen);
	    }

	    // sparkles
	    Spartikle sp[4] = {
	    	{ 0, 91, 39, -10, -8 },
	    	{ 2, 111, 63, -5, -12 },
	    	{ 1, 80, 66, -2, -10 },
	    	{ 3, 89, 50, 3, -8 },
	    };
	    for(unsigned id=0; id<4; ++id) {
	    	sp[id].vx *= 1.5f;
	    	sp[id].vy *= 2.f;
	    	sGame.ViewAt(0)->ResizeSprite(id, 8, 8);
	    }
	    float t = System::clock();
	    do {
		    for(unsigned id=0; id<4; ++id) {
	    		sp[id].Update(0.2f, sGame.ViewAt(0), id);
	    	}
	    	System::paint();
	    } while(System::clock() - t < 2.5f);
	    for(unsigned id=0; id<8; ++id) {
	    	sGame.ViewAt(0)->HideSprite(id);
	    }
	    System::paintSync();
	}
}