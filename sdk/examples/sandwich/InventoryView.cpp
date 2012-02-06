#include "Game.h"

void InventoryView::Init() {
	Parent()->HideSprites();
	ViewMode mode = Parent()->Graphics();
	mode.BG0_drawAsset(Vec2(0,0), InventoryBackground);
}

void InventoryView::Restore() {
	Init();
}

void InventoryView::Update() {
	
}