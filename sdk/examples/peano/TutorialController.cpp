#include "config.h"
#include "TutorialController.h"
#include "AudioPlayer.h"
#include "Game.h"
#include "NarratorView.h"
#include "Puzzle.h"
#include "assets.gen.h"
#include "TokenView.h"
#include "Token.h"
#include "TokenGroup.h"
#include "Skins.h"
#include "ConfirmationMenu.h"

namespace TotalsGame
{

extern Int2 kSideToUnit[4];

namespace TutorialController
{

void OnNeighborAdd(TotalsCube *c, unsigned s, TotalsCube *nc, unsigned ns);
void OnNeighborRemove(TotalsCube *c, unsigned s, TotalsCube *nc, unsigned ns);

class ConnectTwoCubesHorizontalEventHandler: public Game::NeighborEventHandler
{
public:
    void OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1);
};



class ConnectTwoCubesVerticalEventHandler: public Game::NeighborEventHandler
{    
public:
    void OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1);
};


class MakeSixEventHandler: public Game::NeighborEventHandler
{
public:
    void OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1);
    void OnNeighborRemove(unsigned c0, unsigned s0, unsigned c1, unsigned s1);
};


class WaitForShakeEventHandler: public TotalsCube::EventHandler
{
    bool shook;
public:
    WaitForShakeEventHandler();
    void OnCubeShake(TotalsCube *cube);
    bool DidShake();
};

class WaitForTouchEventHanlder: public TotalsCube::EventHandler
{
    bool touched;
public:
    WaitForTouchEventHanlder();
    void OnCubeTouch(TotalsCube *cube, bool _touched);
    bool DidTouch();
};

Puzzle *puzzle;


NarratorView *pNarrator;
TokenView *pFirstToken, *pSecondToken;
ConnectTwoCubesHorizontalEventHandler connectTwoCubesHorizontalEventHandler;
ConnectTwoCubesVerticalEventHandler connectTwoCubesVerticalEventHandler;
MakeSixEventHandler makeSixEventHandler;
WaitForShakeEventHandler waitForShakeEventHandler[2];
WaitForTouchEventHanlder waitForTouchEventHandler[2];

    /* TODO lost and found
     mGame.CubeSet.LostCubeEvent += OnLostCube;
     mGame.CubeSet.NewCubeEvent += OnNewCube;
     */

/*
 void OnLostCube(Cube c) {
 if (!c.IsUnused()) {
 var blanky = mGame.CubeSet.FindAnyIdle();
 if(blanky == null) { throw new Exception("Fatal Lost Cube"); }
 c.MoveViewTo(blanky);
 }
 }

 void OnNewCube(Cube c) {
 new BlankView(c);
 }
 */


void WaitWithTokenUpdate(float delay)
{
    Game::PaintCubeViews();
    System::paint();
    //System::finish();

    SystemTime t = SystemTime::now();

    t += delay;
    do {
        pFirstToken->Update();
        pSecondToken->Update();

        pFirstToken->PaintNow();
        pSecondToken->PaintNow();

        System::paint();
        Game::UpdateDt();
    } while(SystemTime::now() < t);
}


Game::GameState Run() {

    const Skins::Skin &skin = Skins::GetSkin();

    NarratorView narrator;
    TokenView firstToken, secondToken;

    pNarrator = &narrator;
    pFirstToken = &firstToken;
    pSecondToken = &secondToken;


    const float kTransitionDuration = 0.2f; 
    const float period = 0.05f;

    firstToken.Reset();
    secondToken.Reset();
    narrator.Reset();

    PLAY_MUSIC(sfx_PeanosVaultMenu);

    // initialize narrator
    Game::cubes[0].SetView(&narrator);

    // initial blanks
    for(int i = 1; i < NUM_CUBES; i++)
    {
        Game::cubes[i].DrawVaultDoorsClosed();
    }

    Game::Wait(0.5f);


    AudioPlayer::PlayShutterOpen();
    for(float t=0; t<kTransitionDuration; t+=Game::dt) {
        narrator.SetTransitionAmount(t/kTransitionDuration);
        Game::Wait(0);
    }
    narrator.SetTransitionAmount(-1);

    // wait a bit
    Game::Wait(0.5f);
    narrator.SetMessage("Hi there! I'm Peano!", NarratorView::EmoteWave);
    Game::Wait(3);
    narrator.SetMessage("We're going to learn\nto solve secret codes!");
    Game::Wait(3);
    narrator.SetMessage("These codes\nlet you into the\ntreasure vault!", NarratorView::EmoteYay);
    Game::Wait(3);

    // initailize puzzle
    puzzle = new Puzzle(2);
    puzzle->unlimitedHints = true;
    puzzle->GetToken(0)->val = 1;
    puzzle->GetToken(0)->SetOpRight(OpAdd);
    puzzle->GetToken(0)->SetOpBottom(OpSubtract);
    puzzle->GetToken(1)->val = 2;
    puzzle->GetToken(1)->SetOpRight(OpAdd);
    puzzle->GetToken(1)->SetOpBottom(OpSubtract);

    // open shutters
    narrator.SetMessage("");
    Game::Wait(1);
    Game::cubes[1].OpenShuttersToReveal(skin.background);
    // initialize two token views
    firstToken.SetToken(puzzle->GetToken(0));
    Game::cubes[1].SetView(&firstToken);
    firstToken.SetHideMode(TokenView::BIT_BOTTOM | TokenView::BIT_LEFT | TokenView::BIT_TOP | TokenView::BIT_MESSAGE);
    firstToken.PaintNow();
    Game::Wait(0.25f);
    Game::cubes[2].OpenShuttersToReveal(skin.background);
    secondToken.SetToken(puzzle->GetToken(1));
    Game::cubes[2].SetView(&secondToken);
    secondToken.SetHideMode(TokenView::BIT_BOTTOM | TokenView::BIT_RIGHT | TokenView::BIT_TOP | TokenView::BIT_MESSAGE);
    secondToken.PaintNow();
    Game::Wait(0.5f);

    // wait for neighbor
    narrator.SetMessage("These are called Keys.");
    Game::Wait(3);
    narrator.SetMessage("Notice how each Key\nhas a number.");
    Game::Wait(3);

    narrator.SetMessage("Connect two Keys\nto combine them.");
    Game::neighborEventHandler = &connectTwoCubesHorizontalEventHandler;

    while(firstToken.token->current == firstToken.token)
    {
        WaitWithTokenUpdate(0);
    }

    Game::neighborEventHandler = NULL;

    // flourish 1
    WaitWithTokenUpdate(0.5f);  //two waits with token update so overlay turns off
    narrator.SetMessage("Awesome!", NarratorView::EmoteYay);
    WaitWithTokenUpdate(3);
    narrator.SetMessage("The combined Key is\n1+2=3!");
    Game::Wait(3);

    // now show top-down
    narrator.SetMessage("Try connecting Keys\nTop-to-Bottom...");
    {
        TokenGroup *grp = (TokenGroup*)firstToken.token->current;
        delete grp;
        firstToken.token->PopGroup();
        secondToken.token->PopGroup();
        firstToken.DidGroupDisconnect();
        secondToken.DidGroupDisconnect();
        firstToken.SetHideMode(TokenView::BIT_BOTTOM | TokenView::BIT_LEFT | TokenView::BIT_RIGHT | TokenView::BIT_MESSAGE);
        secondToken.SetHideMode(TokenView::BIT_TOP | TokenView::BIT_LEFT | TokenView::BIT_RIGHT | TokenView::BIT_MESSAGE);
    }

    Game::neighborEventHandler = &connectTwoCubesVerticalEventHandler;

    while(firstToken.token->current == firstToken.token)
    {
        WaitWithTokenUpdate(0);
    }

    Game::neighborEventHandler = NULL;

    // flourish 2
    WaitWithTokenUpdate(0.5f);  //two waits with token update so overlay has time to turn off
    narrator.SetMessage("Good job!", NarratorView::EmoteYay);
    WaitWithTokenUpdate(3);
    narrator.SetMessage("See how this\ncombination\nequals 2-1=1.");
    Game::Wait(3);

    // mix up
    narrator.SetMessage("Okay, let's mix\nthem up a little...", NarratorView::EmoteMix01);
    {
        TokenGroup *grp = (TokenGroup*)firstToken.token->current;
        delete grp;
        firstToken.token->PopGroup();
        secondToken.token->PopGroup();
        firstToken.DidGroupDisconnect();
        secondToken.DidGroupDisconnect();
        firstToken.SetHideMode(TokenView::BIT_MESSAGE);
        secondToken.SetHideMode(TokenView::BIT_MESSAGE);
        firstToken.HideOps();
        secondToken.HideOps();
    }

    //set window to bottom half of screen so we can animate peano
    //while text window is open above
    narrator.GetCube()->vid.initMode(BG0_SPR_BG1);
    narrator.GetCube()->vid.setWindow(72,56);

    // press your luck flourish
    {
        PLAY_SFX(sfx_Tutorial_Mix_Nums);

        float timeout = period;
        int cubeId = 0;
        SystemTime t = SystemTime::now() + 3.0;
        while(t > SystemTime::now()) {
            timeout -= Game::dt;
            //while (timeout < 0.0f) {
                cubeId = (cubeId+1) % 2;
            //    timeout += period;
            //}
            (cubeId==0?firstToken:secondToken).PaintRandomNumeral();
            narrator.GetCube()->Image(cubeId?&Narrator_Mix02:&Narrator_Mix01, vec(0, 0), vec(0,3), vec(16,7));
            firstToken.PaintNow();
            secondToken.PaintNow();
            System::paint();
            System::finish();
            Game::UpdateDt();
        }

        narrator.SetMessage("");
        System::paint();

        firstToken.token->val = 2;
        firstToken.token->SetOpRight(OpMultiply);
        secondToken.token->val = 3;
        secondToken.token->SetOpBottom(OpDivide);
        // thread in the real numbers
        int finishCountdown = 2;
        while(finishCountdown > 0) {
            Game::Wait(0);
            timeout -= Game::dt;
            while(finishCountdown > 0 && timeout < 0.0f) {
                --finishCountdown;
                (cubeId==1?firstToken:secondToken).ResetNumeral();
                cubeId++;
                timeout += period;
            }
        }
    }

    // can you make 42?
    narrator.SetMessage("Can you build\nthe number 6?");

    Game::neighborEventHandler = &makeSixEventHandler;


    bool oopsies = false;
    while(firstToken.token->current->GetValue() != Fraction(6))
    {

        if (oopsies && firstToken.token->current == firstToken.token) {
            oopsies = false;
            pNarrator->SetMessage("Can you build\nthe number 6?");
        }

        if(!oopsies && firstToken.token->current != firstToken.token && firstToken.token->current->GetValue() != Fraction(6))
        {
            oopsies = true;
            PLAY_SFX2(sfx_Tutorial_Oops, false);
            pNarrator->SetMessage("Oops,\nlet's try again...", NarratorView::EmoteSad);
        }

        firstToken.Update();
        secondToken.Update();
        Game::UpdateDt();
        System::paint();
    }
    PLAY_SFX(sfx_Tutorial_Correct);
    //PLAY_SFX2(sfx_Tutorial_Oops, false);
    Game::ClearCubeEventHandlers();
    Game::neighborEventHandler = NULL;
    WaitWithTokenUpdate(0.5f);
    narrator.SetMessage("Radical!", NarratorView::EmoteYay);
    WaitWithTokenUpdate(3.5f);

    // close shutters
    narrator.SetMessage("");
    Game::Wait(1);
    //Game::cubes[2]->SetView(NULL);
    Game::cubes[2].Image(Skin_Default_Background);
    Game::cubes[2].vid.bg1.erase();
    Game::cubes[2].HideSprites();
    Game::cubes[2].CloseShutters();
    Game::cubes[2].DrawVaultDoorsClosed();

    Game::cubes[1].Image(Skin_Default_Background);
    Game::cubes[1].vid.bg1.erase();
    Game::cubes[1].HideSprites();
    Game::cubes[1].CloseShutters();
    Game::cubes[1].DrawVaultDoorsClosed();

    Game::Wait(1);
    narrator.SetMessage("Keep combining to build\neven more numbers!", NarratorView::EmoteYay);
    Game::Wait(2);

    Game::cubes[1].OpenShuttersToReveal(Tutorial_Groups);
    Game::cubes[1].Image(Tutorial_Groups, vec(0,0));

    Game::Wait(5);
    narrator.SetMessage("");
    Game::Wait(1);
    Game::cubes[1].CloseShutters();
    Game::cubes[1].DrawVaultDoorsClosed();

    Game::Wait(1);
    narrator.SetMessage("If you get stuck,\nyou can shake\nfor a hint.");
    Game::Wait(2);

    puzzle->target = (TokenGroup*)firstToken.token->current;
    firstToken.token->PopGroup();
    secondToken.token->PopGroup();



    //Game::cubes[1]->SetView(NULL);
    Game::cubes[1].OpenShuttersToReveal(skin.background);
    Game::cubes[1].SetView(&firstToken);
    firstToken.DidGroupDisconnect();
    firstToken.NeedRepaint();
    Game::Wait(0.1f);

    //Game::cubes[2]->SetView(NULL);
    Game::cubes[2].OpenShuttersToReveal(skin.background);
    Game::cubes[2].SetView(&secondToken);
    secondToken.DidGroupDisconnect();
    secondToken.NeedRepaint();
    Game::Wait(1);

    narrator.SetMessage("Try it out!  Shake one!");
    Game::cubes[1].AddEventHandler(&waitForShakeEventHandler[0]);
    Game::cubes[2].AddEventHandler(&waitForShakeEventHandler[1]);
    while(!(waitForShakeEventHandler[0].DidShake()||waitForShakeEventHandler[1].DidShake())) {
        WaitWithTokenUpdate(0);
    }
    Game::ClearCubeEventHandlers();
    WaitWithTokenUpdate(0.5);
    narrator.SetMessage("Nice!", NarratorView::EmoteYay);
    WaitWithTokenUpdate(3);
    narrator.SetMessage("Careful!\nYou only get\na few hints!");
    Game::Wait(3);
    narrator.SetMessage("If you forget\nthe target,\npress the screen.");
    Game::Wait(2);
    narrator.SetMessage("Try it out!  Press one!");

    Game::cubes[1].AddEventHandler(&waitForTouchEventHandler[0]);
    Game::cubes[2].AddEventHandler(&waitForTouchEventHandler[1]);
    while(!(waitForTouchEventHandler[0].DidTouch()||waitForTouchEventHandler[1].DidTouch())) {
        WaitWithTokenUpdate(0);
    }

    WaitWithTokenUpdate(0.5f);
    narrator.SetMessage("Great!", NarratorView::EmoteYay);
    WaitWithTokenUpdate(3);
    firstToken.token->GetPuzzle()->target = NULL;

    Game::cubes[2].HideSprites();
    Game::cubes[2].Image(Skin_Default_Background);
    Game::cubes[2].vid.bg1.erase();
    Game::cubes[2].SetView(NULL);
    Game::cubes[2].CloseShutters();
    Game::cubes[2].DrawVaultDoorsClosed();

    Game::cubes[1].HideSprites();
    Game::cubes[1].Image(Skin_Default_Background);
    Game::cubes[2].vid.bg1.erase();
    Game::cubes[1].CloseShutters();
    Game::cubes[1].DrawVaultDoorsClosed();

    // transition out narrator
    narrator.SetMessage("Let's try it\nfor real, now!", NarratorView::EmoteWave);
    Game::Wait(3);

    //ask user for challenge level
    narrator.SetMessage("What skill level\nare youcomfortable\nstarting with?");
    Game::Wait(0.5f);
    bool choice = ConfirmationMenu::Run(NULL, &Icon_Novice, &Icon_Expert);
    Game::skillLevel = choice? Game::SkillLevel_Novice : Game::SkillLevel_Expert;
    narrator.SetMessage("Thanks!\nYou can always change\nthis in the Settings!");
    Game::Wait(3);

    narrator.SetMessage("Press-and-hold\na cube to access\nthe main menu.");
    Game::Wait(3);
    narrator.SetMessage("Remember, you need\nto use every Key!");
    Game::Wait(3);
    narrator.SetMessage("Good luck!", NarratorView::EmoteWave);
    Game::Wait(3);

    narrator.SetMessage("");

    AudioPlayer::PlayShutterClose();
    Game::cubes[0].SetView(NULL);
    for(float t=0; t<kTransitionDuration; t+=Game::dt) {
        narrator.SetTransitionAmount(1.0f-t/kTransitionDuration);
        Game::Wait(0);
    }
    Game::cubes[0].DrawVaultDoorsClosed();

    Game::Wait(0.5);

    delete puzzle->target;
    delete puzzle;

    Game::ClearCubeEventHandlers();
    Game::ClearCubeViews();

    //free up our tokens
    Token::ResetAllocationPool();
    TokenGroup::ResetAllocationPool();

    Game::saveData.CompleteTutorial();
    if (Game::currentPuzzle == NULL) {
        if (!Game::saveData.AllChaptersSolved()) {
            Game::currentPuzzle = Game::saveData.FindNextPuzzle();
        } else {
            Game::currentPuzzle = Database::GetPuzzleInChapter(0, 0);
        }
    }

    return Game::GameState_Interstitial;


}

void OnNeighborAdd(TotalsCube *c, unsigned s, TotalsCube *nc, unsigned ns) {

    // used to make sure token views attached to cubes.
    // now this function is only called from one place (makesixeventhandler)
    // and we know that if its cube 1 and 2 they have token views

    if(s != ((ns+2)%4))
    {
        //opposing faces check
        return;
    }

    if (s == LEFT || s == TOP) {
        OnNeighborAdd(nc, ns, c, s);
        return;
    }

    if(!((c->sys == 1 || c->sys == 2) && (nc->sys == 1 || nc->sys == 2)))
    {
        //cubes 1 and 2 check
        return;
    }


    TokenView *v = (TokenView*)c->GetView();
    TokenView *nv = (TokenView*)nc->GetView();

    Token *t = v->token;
    Token *nt = nv->token;
    // resolve solution
    if (t->current != nt->current) {
        TokenGroup *grp = TokenGroup::Connect(t, kSideToUnit[s], nt);
        if (grp != NULL) {
            v->WillJoinGroup();
            nv->WillJoinGroup();
            grp->SetCurrent(grp);
            v->DidJoinGroup();
            nv->DidJoinGroup();
        }
    }
    //AudioPlayer::PlayNeighborAdd();
}

void OnNeighborRemove(TotalsCube *c, unsigned s, TotalsCube *nc, unsigned ns) {

    // validate args
    if(!((c->sys == 1 || c->sys == 2) && (nc->sys == 1 || nc->sys == 2)))
    {
        //cubes 1 and 2 check
        return;
    }


    Token *t = pFirstToken->token;
    Token *nt = pSecondToken->token;
    // resolve soln
    if (t->current == NULL || nt->current == NULL || t->current != nt->current) {
        return;
    }
    TokenGroup *grp = (TokenGroup*)t->current;
    t->PopGroup();
    nt->PopGroup();
    pFirstToken->DidGroupDisconnect();
    pSecondToken->DidGroupDisconnect();
    delete grp;
    //Jukebox.PlayNeighborRemove();
}





    void ConnectTwoCubesHorizontalEventHandler::OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1)
    {
        if ((s0 == RIGHT && s1 == LEFT && c0 == pFirstToken->GetCube()->sys && c1 == pSecondToken->GetCube()->sys)
            ||(s0 == LEFT && s1 == RIGHT && c1 == pFirstToken->GetCube()->sys && c0 == pSecondToken->GetCube()->sys))
        {
            Game::neighborEventHandler = NULL;
            TokenGroup *grp = TokenGroup::Connect(pFirstToken->token, kSideToUnit[RIGHT], pSecondToken->token);
            if (grp != NULL)
            {
                pFirstToken->WillJoinGroup();
                pSecondToken->WillJoinGroup();
                grp->SetCurrent(grp);
                pFirstToken->DidJoinGroup();
                pSecondToken->DidJoinGroup();
            }
            PLAY_SFX(sfx_Tutorial_Correct);
        }
    }





    void ConnectTwoCubesVerticalEventHandler::OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1)
    {
        if ((s0 == TOP && s1 == BOTTOM && c0 == pFirstToken->GetCube()->sys && c1 == pSecondToken->GetCube()->sys)
            ||(s0 == BOTTOM && s1 == TOP && c1 == pFirstToken->GetCube()->sys && c0 == pSecondToken->GetCube()->sys))
        {
            Game::neighborEventHandler = NULL;
            TokenGroup *grp = TokenGroup::Connect(pFirstToken->token, kSideToUnit[TOP], pSecondToken->token);
            if (grp != NULL)
            {
                pFirstToken->WillJoinGroup();
                pSecondToken->WillJoinGroup();
                grp->SetCurrent(grp);
                pFirstToken->DidJoinGroup();
                pSecondToken->DidJoinGroup();
            }
            PLAY_SFX(sfx_Tutorial_Correct);
        }
    }




    void MakeSixEventHandler::OnNeighborAdd(unsigned c0, unsigned s0, unsigned c1, unsigned s1)
    {
        TutorialController::OnNeighborAdd(&Game::cubes[c0], s0, &Game::cubes[c1], s1);
    }

    void MakeSixEventHandler::OnNeighborRemove(unsigned c0, unsigned s0, unsigned c1, unsigned s1)
    {
        TutorialController::OnNeighborRemove(&Game::cubes[c0],s0, &Game::cubes[c1], s1);
    }



    WaitForShakeEventHandler::WaitForShakeEventHandler()
    {
        shook = false;
    }

    void WaitForShakeEventHandler::OnCubeShake(TotalsCube *cube)
    {
        shook = true;
    }

    bool WaitForShakeEventHandler::DidShake()
    {
        return shook;
    }



    WaitForTouchEventHanlder::WaitForTouchEventHanlder()
    {
        touched = false;
    }

    void WaitForTouchEventHanlder::OnCubeTouch(TotalsCube *cube, bool _touched)
    {
        touched = touched | _touched;
    }

    bool WaitForTouchEventHanlder::DidTouch()
    {
        return touched;
    }






}

}
