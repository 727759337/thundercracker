#include "Game.h"
#include "DrawingHelpers.h"

#define HOVERING_ICON_ID	0

void InventoryView::Init() {
	CORO_RESET;
	mSelected = 0;
	Int2 tilt = Parent()->GetCube()->virtualAccel();
	mTilt.set(tilt.x, tilt.y);
	mAccum.set(0,0);
	mTouch = Parent()->GetCube()->touching();
	mAnim = 0;
	Parent()->HideSprites();
	Parent()->Graphics().BG0_drawAsset(vec(0,0), InventoryBackground);
	RenderInventory();
}

void InventoryView::Restore() {
	mAccum.set(0,0);
	mTouch = Parent()->GetCube()->touching();
	Parent()->HideSprites();
	Parent()->Graphics().BG0_drawAsset(vec(0,0), InventoryBackground);
	RenderInventory();
}

static const char* kLabels[4] = { "top", "left", "bottom", "right" };
int t;

void InventoryView::Update() {
	bool touch = UpdateTouch();
	CORO_BEGIN;
	while(1) {
		do {
			if (gGame.AnimFrame()%2==0) {
				ComputeHoveringIconPosition();
			}
			{
				Cube::Side side = UpdateAccum();
				if (side != SIDE_UNDEFINED) {
                    Int2 pos = vec(mSelected % 4, mSelected >> 2) + kSideToUnit[side].toInt();
					int idx = pos.x + (pos.y<<2);
					uint8_t items[16];
					int count = gGame.GetState()->GetItems(items);
					if (idx >= 0 && idx < count) {
						mSelected = idx;
						mAnim = 0;
						RenderInventory();
					}
				}
			}
			CORO_YIELD;
		} while(!touch);

		gGame.NeedsSync();
		CORO_YIELD;
		#if GFX_ARTIFACT_WORKAROUNDS		
			Parent()->GetCube()->vbuf.touch();
			CORO_YIELD;
			gGame.NeedsSync();
			Parent()->GetCube()->vbuf.touch();
			CORO_YIELD;
			gGame.NeedsSync();
			Parent()->GetCube()->vbuf.touch();
			CORO_YIELD;
		#endif
		{
			uint8_t items[16];
			int count = gGame.GetState()->GetItems(items);
			//Parent()->Graphics().setWindow(80, 48);
			Parent()->Graphics().setWindow(80+16,128-80-16);
			mDialog.Init(Parent()->GetCube());
			mDialog.Erase();
			mDialog.ShowAll(gItemTypeData[items[mSelected]].description);
		}
		gGame.NeedsSync();
		Parent()->GetCube()->vbuf.touch();
		CORO_YIELD;
		for(t=0; t<16; t++) {
			Parent()->Graphics().setWindow(80+15-(t),128-80-15+(t));
			mDialog.SetAlpha(t<<4);
			CORO_YIELD;
		}
		mDialog.SetAlpha(255);
		while(Parent()->GetCube()->touching()) {
			CORO_YIELD;	
		}
		System::paintSync();
		Parent()->Restore();
		mAccum.set(0,0);
		gGame.NeedsSync();
		CORO_YIELD;
		gGame.NeedsSync();
		Parent()->GetCube()->vbuf.touch();
		CORO_YIELD;
	}


	CORO_END;
}

void InventoryView::OnInventoryChanged() {
	RenderInventory();
}

void InventoryView::RenderInventory() {
	BG1Helper overlay(*Parent()->GetCube());

	const int pad = 24;
	const int innerPad = (128-pad-pad)/3;
	uint8_t items[16];
	unsigned count = gGame.GetState()->GetItems(items);
	for(unsigned i=0; i<count; ++i) {
		const int x = i % 4;
		const int y = i >> 2;
		if (i == mSelected) {
			overlay.DrawAsset(vec(x<<2,y<<2), InventoryReticle);
		} else {
			overlay.DrawAsset(vec(1 + (x<<2),1 + (y<<2)), Items, items[i]);
		}
	}
	overlay.Flush();	
	ViewMode gfx = Parent()->Graphics();
	gfx.resizeSprite(HOVERING_ICON_ID, vec(16, 16));
	gfx.setSpriteImage(HOVERING_ICON_ID, Items, items[mSelected]);
	ComputeHoveringIconPosition();
	gGame.NeedsSync();
}

void InventoryView::ComputeHoveringIconPosition() {
	mAnim++;
	Parent()->Graphics().moveSprite(
		HOVERING_ICON_ID, 
		8 + (mSelected%4<<5), 
		8 + ((mSelected>>2)<<5) + kHoverTable[ mAnim % HOVER_COUNT]
	);
}

Cube::Side InventoryView::UpdateAccum() {
	const int radix = 8;
	const int threshold = 128;
	Int2 tilt = Parent()->GetCube()->virtualAccel();
	mTilt = tilt;
	Int2 delta = tilt / radix;
	if (delta.x) {
		mAccum.x += delta.x;
	} else {
		mAccum.x = 0;
	}
	if (delta.y) {
		mAccum.y += delta.y;
	} else {
		mAccum.y = 0;
	}
	if (delta.x) {
		if (mAccum.x >= threshold) {
			mAccum.x %= threshold;
			return SIDE_RIGHT;
		} else if (mAccum.x <= -threshold) {
			mAccum.x %= threshold;
			return SIDE_LEFT;
		} 
	}
	if (delta.y) {
		if (mAccum.y >= threshold) {
			mAccum.y %= threshold;
			return SIDE_BOTTOM;
		} else if (mAccum.y <= -threshold) {
			mAccum.y %= threshold;
			return SIDE_TOP;
		}		
	}
	return SIDE_UNDEFINED;
}

bool InventoryView::UpdateTouch() {
	bool touch = Parent()->GetCube()->touching();
	if (touch != mTouch) {
		mTouch = touch;
		return mTouch;
	}
	return false;
}
