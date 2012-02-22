#include "InterstitialController.h"

namespace TotalsGame {

    InterstitialController::InterstitialController (Game *game) {
        mGame = game;
        iv = NULL;
    }

    void InterstitialController::OnSetup() {
        CORO_RESET;
        mDone = false;
        /* TODO
      mGame.CubeSet.NewCubeEvent += cube => new BlankView(cube);
      mGame.CubeSet.LostCubeEvent += delegate(Cube cube) {
        if (!mDone && cube.userData is InterstitialView) {
          mDone = true;
          mGame.sceneMgr.QueueTransition("Next");
        }
      };
      */
    }

    float InterstitialController::Coroutine(float dt) {

        const float kTransitionTime = 0.5f;
        static char ivBuffer[sizeof(InterstitialView)];
        static char blankViewBuffer[Game::NUMBER_OF_CUBES][sizeof(BlankView)];

        CORO_BEGIN;

        iv = new(ivBuffer) InterstitialView(Game::GetCube(0));
        if (mGame->IsPlayingRandom()) {
            iv->message = "Random!";
            iv->image = &Hint_6;
        } else {
            iv->message = mGame->currentPuzzle->chapter->name;
            static const PinnedAssetImage *hints[] =
            {
                &Hint_0,&Hint_1,&Hint_2,&Hint_3,&Hint_4,&Hint_5,&Hint_6
            };
            iv->image = hints[mGame->database.IndexOfChapter(mGame->currentPuzzle->chapter)];
        }

        for(int i = 1; i < Game::NUMBER_OF_CUBES; i++)
        {
            new(blankViewBuffer[i]) BlankView(Game::GetCube(i), NULL);
        }

        CORO_YIELD(0.333f);
        AudioPlayer::PlayShutterOpen();
        for(remembered_t=0; remembered_t<kTransitionTime; remembered_t+=mGame->dt) {
            iv->SetTransitionAmount(remembered_t/kTransitionTime);
            CORO_YIELD(0);
        }
        iv->SetTransitionAmount(1);
        AudioPlayer::PlaySfx(sfx_Tutorial_Correct);
        CORO_YIELD(3);
        AudioPlayer::PlayShutterClose();
        for(remembered_t=0; remembered_t<kTransitionTime; remembered_t+=mGame->dt) {
            iv->SetTransitionAmount(1.0f-remembered_t/kTransitionTime);
            CORO_YIELD(0);
        }
        new(blankViewBuffer[0]) BlankView(Game::GetCube(0), NULL);
        CORO_YIELD(0.333f);

        CORO_END;
        return -1;
    }

    void InterstitialController::OnTick (float dt) {        
        if (mDone) { return; }
        UPDATE_CORO(Coroutine, dt);
        if(mTimer == -1) {  //result of update coro stored in 'secret' variable
            mDone = true;
            mGame->sceneMgr.QueueTransition("Next");
        } else {
            Game::UpdateCubeViews(dt);
        }
    }

    void InterstitialController::OnPaint (bool canvasDirty) {
        if (canvasDirty) { Game::PaintCubeViews(); }
    }

    void InterstitialController::OnDispose () {
        Game::ClearCubeEventHandlers();
        Game::ClearCubeViews();
    }



}

