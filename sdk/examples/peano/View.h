#pragma once

#include "sifteo.h"
#include "ObjectPool.h"

namespace TotalsGame
{
	class TotalsCube;

	class View
	{	
		DECLARE_POOL(View, 0)		

		TotalsCube *mCube;

	public:
		View(TotalsCube *_cube);

		virtual void Paint() {}
		virtual void Update(float dt) {}
		virtual void DidAttachToCube(TotalsCube *c) {}
		virtual void WillDetachFromCube(TotalsCube *c) {}
		virtual ~View() {};

		static void PaintViews(TotalsCube *cubes, int numCubes);

		void SetCube(TotalsCube *c);
		TotalsCube *GetCube();

        static bool OkayToPaint(){return true;} //todo member of view perhaps?
	};

}
