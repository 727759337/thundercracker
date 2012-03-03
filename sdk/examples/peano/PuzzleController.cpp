#include "PuzzleController.h"
#include "BlankView.h"
#include "assets.gen.h"
#include "PauseHelper.h"
#include "ConfirmationMenu.h"

namespace TotalsGame {

namespace PuzzleController
{

class EventHandler: public TotalsCube::EventHandler
{
    void OnCubeShake(TotalsCube *cube) {};
    void OnCubeTouch(TotalsCube *cube, bool touching) {};
    //TODO connect, disconnect
};
EventHandler eventHandlers[NUM_CUBES];

class NeighborEventHandler: public Game::NeighborEventHandler
{
    void OnNeighborAdd(Cube::ID c0, Cube::Side s0, Cube::ID c1, Cube::Side s1);
    void OnNeighborRemove(Cube::ID c0, Cube::Side s0, Cube::ID c1, Cube::Side s1);
};
NeighborEventHandler neighborEventHandler;


Puzzle *puzzle;
bool paused;


void OnSetup ()
{
    bool paused = false;
    PauseHelper *pauseHelper = NULL;

    AudioPlayer::PlayInGameMusic();

    String<10> stringRep;
    int stringRepLength;
    if (Game::IsPlayingRandom())
    {
        int nCubes = 3 + ( NUM_CUBES > 3 ? Random().randrange(NUM_CUBES-3+1) : 0 );
        puzzle = PuzzleHelper::SelectRandomTokens(Game::difficulty, nCubes);
        // hacky reliability :P
        int retries = 0;
        do
        {
            if (retries == 100)
            {
                retries = 0;
                delete puzzle;  //selectrandomtokens does a new
                puzzle = PuzzleHelper::SelectRandomTokens(Game::difficulty, nCubes);
            }
            else
            {
                TokenGroup::ResetAllocationPool();
                retries++;
            }
            puzzle->SelectRandomTarget();
            stringRep.clear();
            puzzle->target->GetValue().ToString(&stringRep);
            stringRepLength = stringRep.size();
        } while(stringRepLength > 4 || puzzle->target->GetValue().IsNan());
    }
    else
    {
        puzzle = Game::currentPuzzle;
        puzzle->ClearGroups();
        puzzle->target->RecomputeValue(); // in case the difficulty changed
    }
    paused = false;
    puzzle->hintsUsed = 0;

    for(int i = 0; i < NUM_CUBES; i++)
    {
        Game::cubes[0].AddEventHandler(&eventHandlers[i]);
    }

    //TODO game.CubeSet.LostCubeEvent += OnCubeLost;
}

/* TODO
         on cube found put a blank view on it
         void OnCubeLost(Cube c) {
         if (!c.IsUnused()) {
         // for now, let's just return to the main menu
         Transition("Quit");
         }
         }
         */
//-------------------------------------------------------------------------
// MAIN CORO
//-------------------------------------------------------------------------

Game::GameState Coroutine()
{
    OnSetup();

    PauseHelper *pauseHelper;

    bool transitioningOut;

    static char blankViewBuffer[NUM_CUBES][sizeof(BlankView)];
    static char tokenViewBuffer[NUM_CUBES][sizeof(TokenView)];
    static char pauseHelperBuffer[sizeof(PauseHelper)];


    for(int i = puzzle->GetNumTokens(); i < NUM_CUBES; i++)
    {
        new(blankViewBuffer[i]) BlankView(&Game::cubes[0], NULL);
    }
    Game::DrawVaultDoorsClosed();
    CORO_YIELD(0.25);

    for(int i = 0; i < puzzle->GetNumTokens(); i++)
    {
        Game::cubes[i].OpenShuttersSync(&Background);
        {
            TokenView *tv = new(tokenViewBuffer[i]) TokenView(&Game::cubes[i], puzzle->GetToken(i), true);
            tv->PaintNow();
        }

        CORO_YIELD(0.1f);

    }

    { // flourish in
        CORO_YIELD(1.1f);
        // show total
        PLAY_SFX(sfx_Tutorial_Mix_Nums);

        for(int i = 0; i < puzzle->GetNumTokens(); i++)
        {
            puzzle->GetToken(i)->GetTokenView()->ShowOverlay();
        }
        CORO_YIELD(3.0f);
        for(int i = 0; i < puzzle->GetNumTokens(); i++)
        {
            puzzle->GetToken(i)->GetTokenView()->HideOverlay();
        }
    }

    { // gameplay
        pauseHelper = new(pauseHelperBuffer) PauseHelper();

        // subscribe to events
        Game::neighborEventHandler = &neighborEventHandler;
        { // game loop
            while(!puzzle->IsComplete()) {
                // most stuff is handled by events and view updaters
                CORO_YIELD(0);

                // should pause?
                pauseHelper->Update();
                if (pauseHelper->IsTriggered())
                {
                    paused = true;

                    // transition out
                    for(int t=0; t<puzzle->GetNumTokens(); ++t)
                    {
                        for(int i = 0; i < _SYS_VRAM_SPRITES; i++)
                        {
                            Game::cubes[t].backgroundLayer.hideSprite(i);
                        }
                        Game::cubes[t].foregroundLayer.Clear();
                        Game::cubes[t].foregroundLayer.Flush();
                        Game::cubes[t].SetView(NULL);

                        Game::cubes[t].CloseShuttersSync(&Background);
                        new(blankViewBuffer[t]) BlankView(&Game::cubes[t], NULL);
                    }

                    // do menu
                    if (ConfirmationMenu::Coroutine("Return to Menu?"))
                    {
                        Game::neighborEventHandler = NULL;
                        return Game::GameState_Menu;
                    }


                    // transition back
                    for(int i=0; i<puzzle->GetNumTokens(); ++i)
                    {
                        Game::cubes[i].SetView(NULL);

                        Game::cubes[i].OpenShuttersSync(&Background);

                        Game::cubes[i].SetView(puzzle->GetToken(i)->GetTokenView());
                        ((TokenView*)Game::cubes[i].GetView())->PaintNow();
                        CORO_YIELD(0.1f);
                    }

                    pauseHelper->Reset();
                    paused = false;
                }
            }
        }

    }
    // unsubscribe
    Game::neighborEventHandler = NULL;


    { // flourish out
        CORO_YIELD(1);
        PLAY_SFX(sfx_Level_Clear);
        for(int i = 0; i < puzzle->GetNumTokens(); i++)
        {
            puzzle->GetToken(i)->GetTokenView()->ShowLit();
        }
        CORO_YIELD(3);
    }

    { // transition out
        for(int i = 0; i < puzzle->GetNumTokens(); i++)
        {
            puzzle->GetToken(i)->GetTokenView()->GetCube()->foregroundLayer.Clear();
            puzzle->GetToken(i)->GetTokenView()->GetCube()->foregroundLayer.Flush();

            Game::cubes[i].CloseShuttersSync(&Background);
            CORO_YIELD(0.1f);
        }
    }

    if (!Game::IsPlayingRandom())
    { // closing remarks
        int count = puzzle->CountAfterThisInChapterWithCurrentCubeSet();
        if (count > 0)
        {
#if 0 //todo
            var nv = new NarratorView();
            nv.Cube = game.CubeSet[0];
            const float kTransitionTime = 0.2f;
            Jukebox.PlayShutterOpen();
            for(var t=0f; t<kTransitionTime; t+=game.dt) {
                nv.SetTransitionAmount(t/kTransitionTime);
                yield return 0;
            }
            nv.SetTransitionAmount(1f);
            yield return 0.5f;
            if (count == 1) {
                nv.SetMessage("1 code to go...", "mix01");
                int i=0;
                yield return 0;
                for(var t=0f; t<3f; t+=game.dt) {
                    i = 1-i;
                    nv.SetEmote("mix0"+(i+1));
                    yield return 0;
                }
            } else {
                nv.SetMessage(string.Format("{0} codes to go...", count));
                yield return 2f;
            }
            Jukebox.PlayShutterClose();
            for(var t=0f; t<kTransitionTime; t+=game.dt) {
                nv.SetTransitionAmount(1f-t/kTransitionTime);
                yield return 0;
            }
            nv.SetTransitionAmount(0f);
#endif
        }
    }

    Game::ClearCubeEventHandlers();
    Game::ClearCubeViews();
    delete puzzle;

    return Game::GameState_Advance;
}

//-------------------------------------------------------------------------
// EVENTS
//-------------------------------------------------------------------------
void PuzzleController::NeighborEventHandler::OnNeighborAdd(Cube::ID c0, Cube::Side s0, Cube::ID c1, Cube::Side s1)
{
    if (paused /*TODO|| mRemoveEventBuffer.Count > 0*/) { return; }

    // validate args
    TokenView *v = (TokenView*)Game::cubes[c0].GetView();
    TokenView *nv = (TokenView*)Game::cubes[c1].GetView();
    if (v == NULL || nv == NULL) { return; }
    if (s0 != ((s1+2)%NUM_SIDES)) { return; }
    if (s0 == SIDE_LEFT || s0 == SIDE_TOP)
    {
        OnNeighborAdd(c1, s1, c0, s0);
        return;
    }
    Token *t = v->token;
    Token *nt = nv->token;
    // resolve solution
    if (t->current != nt->current)
    {
        AudioPlayer::PlayNeighborAdd();
        TokenGroup *grp = TokenGroup::Connect(t->current, t, kSideToUnit[s0], nt->current, nt);
        if (grp != NULL) {
            grp->AlertWillJoinGroup();
            grp->SetCurrent(grp);
            grp->AlertDidJoinGroup();                }
    }
}

void PuzzleController::NeighborEventHandler::OnNeighborRemove(Cube::ID c0, Cube::Side s0, Cube::ID c1, Cube::Side s1)
{
    /*TODO          if (IsPaused() TODO|| mRemoveEventBuffer.Count > 0)
             {
             mRemoveEventBuffer.Add(new RemoveEvent() { c = c, s = s, nc = nc, ns = ns });
             return;
             }
             */

    // validate args
    TokenView *v = (TokenView*)Game::cubes[c0].GetView();
    TokenView *nv = (TokenView*)Game::cubes[c1].GetView();
    if (v == NULL || nv == NULL) { return; }
    Token *t = v->token;
    Token *nt = nv->token;
    // resolve soln
    if (t->current == NULL || nt->current == NULL || t->current != nt->current)
    {
        return;
    }
    AudioPlayer::PlayNeighborRemove();
    TokenGroup *grp = (TokenGroup*)t->current;
    while(t->current->Contains(nt)) {
        for(int i = 0; i < puzzle->GetNumTokens(); i++)
        {
            Token *token = puzzle->GetToken(i);
            if (token != t && token->current == t->current) {
                token->PopGroup();
            }
        }
        IExpression *current = t->current;
        t->PopGroup();
        if(current != grp && current->IsTokenGroup())
        {   //save the root delete until after the below alertDidDisconnect
            delete (TokenGroup*)current;
        }
    }
    grp->AlertDidGroupDisconnect();
    delete grp;
}
/*TODO
         void ProcessRemoveEventBuffer() {
         var args = mRemoveEventBuffer;
         mRemoveEventBuffer = new List<RemoveEvent>(args.Count);
         foreach(var arg in args) {
         OnNeighborRemove(arg.c, arg.s, arg.nc, arg.ns);
         }
         } */



}


}
