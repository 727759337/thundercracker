#pragma once
#include "View.h"

class IdleView : public View {
private:
    unsigned mStartFrame;
    unsigned mCount;


public:
	void Init();
	void Restore();
	//void OnInventoryChanged();

private:
	//void DrawInventorySprites();
};