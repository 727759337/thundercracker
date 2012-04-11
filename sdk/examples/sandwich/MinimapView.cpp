#include "Game.h"

#define SPRITE_DOT_ID	0

void MinimapView::Init() {
	Parent()->HideSprites();
	Parent()->Video().bg1.eraseMask();
	VideoBuffer& g = Parent()->Video();
	Map *pMap = gGame.GetMap();
	const MapData* pData = pMap->Data();
	unsigned padLeft = (16 - pData->width) >> 1;
	unsigned padTop = (16 - pData->height) >> 1;

	// replace drawAsset w/ putTile?

	// fill in the top
	for(unsigned row=0; row<padTop; ++row) {
		for(unsigned col=0; col<18; ++col) {
			g.bg0.image(vec(col, row), BlackTile);
		}
	}

	// fill in the map
	for(unsigned row=0; row<pData->height; ++row) {
		unsigned y = row + padTop;
		// fill in the left
		for(unsigned col=0; col<padLeft; ++col) {
			g.bg0.image(vec(col, y), BlackTile);
		}
		// fill in the map data
		for(unsigned col=0; col<pData->width; ++col) {
			unsigned x = col + padLeft;
			g.bg0.image(vec(x,y), MinimapBasic, ComputeTileId(col, row));
		}

		// fill in the right
		for (unsigned col=padLeft+pData->width; col<18; ++col) {
			g.bg0.image(vec(col, y), BlackTile);
		}
	}

	// fill in the bottom
	for(unsigned row=padTop+pData->height; row<18; ++row) {
		for(unsigned col=0; col<18; ++col) {
			g.bg0.image(vec(col, row), BlackTile);
		}
	}

	Int2 pan = vec(-((pData->width%2)<<2), -((pData->height%2)<<2));
	g.bg0.setPanning(pan);

	mCanvasOffset.x = 8 * padLeft - pan.x - 4;
	mCanvasOffset.x = 8 * padTop - pan.y - 4;

	g.sprites[SPRITE_DOT_ID].resize(8, 8);
	g.sprites[SPRITE_DOT_ID].setImage(MinimapDot);
	g.sprites[SPRITE_DOT_ID].move(
		(gGame.GetPlayer()->Position()<<3) / 128 + mCanvasOffset.toInt()
	);
}

void MinimapView::Restore() {
	Init();
}

void MinimapView::Update() {
	Parent()->Video().sprites[SPRITE_DOT_ID].move(
		(gGame.GetPlayer()->Position()<<3) / 128 + mCanvasOffset.toInt()
	);
}

unsigned MinimapView::ComputeTileId(int lx, int ly) {
	Map *pMap = gGame.GetMap();
	unsigned t = ly > 0 && pMap->GetPortalY(lx, ly-1);
	unsigned l = lx > 0 && pMap->GetPortalX(lx-1, ly);
	unsigned b = ly < pMap->Data()->height-1 && pMap->GetPortalY(lx, ly);
	unsigned r = lx < pMap->Data()->width-1 && pMap->GetPortalX(lx, ly);
	return (t) | (l<<1) | (b<<2) | (r<<3);
}