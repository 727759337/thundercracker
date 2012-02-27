#include "StingController.h"

namespace TotalsGame 
{


void StingController::EventHandler::OnCubeTouch(TotalsCube *cube, bool touching)
{
    if(!touching)
        owner->Skip();
}

void StingController::EventHandler::OnCubeShake(TotalsCube *cube)
{
    owner->Skip();
}

StingController::StingController(Game *game)
{
    for(int i = 0; i < Game::NUMBER_OF_CUBES; i++)
    {
        eventHandlers[i].owner = this;
    }

    mGame = game;
}

//IStateController

void StingController::OnSetup ()
{
    CORO_RESET;

    //TODO
    //mGame.CubeSet.LostCubeEvent += delegate { Skip(); };
    //mGame.CubeSet.NewCubeEvent += delegate { Skip(); };
}

void StingController::Skip()
{
    mGame->sceneMgr.QueueTransition("Next");
}

float StingController::Coroutine(float dt)
{

    static char blankViewBuffer[Game::NUMBER_OF_CUBES][sizeof(BlankView)];

    CORO_BEGIN;

    for( i = 0; i < Game::NUMBER_OF_CUBES; i++)
    {
        new(blankViewBuffer[i]) BlankView(&Game::GetInstance().cubes[i], NULL);
        Game::GetCube(i)->AddEventHandler(&eventHandlers[i]);
    }

    CORO_YIELD(0.1f);
    AudioPlayer::PlaySfx(sfx_Stinger_02);

    for(i = 0; i < Game::NUMBER_OF_CUBES; i++)
    {
        float dt;
        while((dt=Game::GetCube(i)->OpenShutters(&Title)) >= 0)
        {
            //CORO_YIELD(dt);
            System::paintSync();
            Game::GetInstance().UpdateDt();
        }
        ((BlankView*)Game::GetCube(i)->GetView())->SetImage(&Title);
        CORO_YIELD(0);
    }

    CORO_YIELD(3.0);

    for(i = 0; i < Game::NUMBER_OF_CUBES; i++)
    {
        float dt;
        while((dt=Game::GetCube(i)->CloseShutters(&Title)) >= 0)
        {
            //CORO_YIELD(dt);
            System::paintSync();
            Game::GetInstance().UpdateDt();
        }
        ((BlankView*)Game::GetCube(i)->GetView())->SetImage(NULL);
        CORO_YIELD(0);
    }

    CORO_YIELD(0.5f);

    mGame->sceneMgr.QueueTransition("Next");

    CORO_END

            return -1;
}

void StingController::OnTick (float dt)
{
    UPDATE_CORO(Coroutine, dt);
    Game::UpdateCubeViews(dt);
}

void StingController::OnPaint (bool canvasDirty)
{
    if (canvasDirty) { View::PaintViews(Game::GetInstance().cubes, Game::NUMBER_OF_CUBES); }
}

void StingController::OnDispose ()
{
    //TODO purge pending events?
    Game::ClearCubeEventHandlers();
    Game::ClearCubeViews();

}

}

