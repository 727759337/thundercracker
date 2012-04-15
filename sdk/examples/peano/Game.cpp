#include "config.h"

#include "sifteo.h"
#include "Game.h"

#include "StingController.h"
#include "PuzzleController.h"
#include "MenuController.h"
#include "InterstitialController.h"
#include "TutorialController.h"
#include "VictoryController.h"
#include "Skins.h"

namespace TotalsGame
{
namespace Game
{
NeighborEventHandler *neighborEventHandler;


TotalsCube cubes[NUM_CUBES];

Puzzle *currentPuzzle;
Puzzle *previousPuzzle;

Random rand;
int randomPuzzleCount;
SkillLevel skillLevel = SkillLevel_Expert;
SaveData saveData;

float dt;
TimeStep timeStep;

void OnNeighborAdd(void*, unsigned c0, unsigned s0, unsigned c1, unsigned s1)
{
    if(neighborEventHandler)
        neighborEventHandler->OnNeighborAdd(c0, s0, c1, s1);
}

void OnNeighborRemove(void*, unsigned c0, unsigned s0, unsigned c1, unsigned s1)
{
    {
        if(neighborEventHandler)
            neighborEventHandler->OnNeighborRemove(c0, s0, c1, s1);
    }

}
    
void OnCubeTouch(void*, unsigned cid)
{
    TotalsCube *c = &Game::cubes[cid];
    c->DispatchOnCubeTouch(c, c->isTouching());
}

void OnCubeShake(void*, unsigned cid)
{
    TotalsCube *cube = &Game::cubes[cid];
    cube->DispatchOnCubeShake(cube);
}

    
#if NO_TOUCH_HACK
//tilt y to touch
void OnCubeTilt(void*, unsigned cid)
{
    static int oldState = _SYS_TILT_NEUTRAL;
    _SYSTiltState ts = cubes[cid].getTiltState();
    if(ts.y != oldState)
    {
        cubes[cid].DispatchOnCubeTouch(cubes+cid, ts.y != _SYS_TILT_NEUTRAL);
        oldState = ts.y;
    }
}
#endif

    
void ClearCubeViews()
{
    for(int i = 0; i < NUM_CUBES; i++)
    {
        cubes[i].SetView(NULL);
    }
}

void ClearCubeEventHandlers()
{
    for(int i = 0; i < NUM_CUBES; i++)
    {
        cubes[0].ResetEventHandlers();
    }
}

void DrawVaultDoorsClosed()
{
    for(int i = 0; i < NUM_CUBES; i++)
    {
        cubes[i].DrawVaultDoorsClosed();
    }
}

void PaintCubeViews()
{
    for(int i = 0; i < NUM_CUBES; i++)
    {
        TotalsCube *c = &cubes[i];
        if(c && !c->IsTextOverlayEnabled())
        {
            View *v = c->GetView();
            if(v)
                v->Paint();
        }
    }
}

ExperienceLevel GetExperienceLevel()
{
    int count = 0;
    for(int i = 0; i < Database::NumChapters(); i++)
    {
        if(saveData.IsChapterSolved(i))
        {
            count++;
        }
    }
    return count > 1 ? ExperienceLevel_Master : ExperienceLevel_Neophyte;
}

Difficulty GetDifficulty()
{
    if (skillLevel == SkillLevel_Novice)
    {
        return GetExperienceLevel() == ExperienceLevel_Neophyte ? DifficultyEasy : DifficultyMedium;
    }
    else
    {
        return GetExperienceLevel() == ExperienceLevel_Neophyte ? DifficultyMedium : DifficultyHard;
    }
}

void SaveOptions()
{
    //TODO
}

void Run()
{    
    for(int i = 0; i < NUM_CUBES; i++)
    {
        cubes[i].Init(i);
        cubes[i].vid.initMode(BG0_SPR_BG1);
    }

    AudioPlayer::Init();

    _SYS_setVector(_SYS_NEIGHBOR_ADD , (void*)&OnNeighborAdd, NULL);
    _SYS_setVector(_SYS_NEIGHBOR_REMOVE , (void*)&OnNeighborRemove, NULL);
    _SYS_setVector(_SYS_CUBE_TOUCH, (void*)&OnCubeTouch, NULL);
    _SYS_setVector(_SYS_CUBE_SHAKE, (void*)&OnCubeShake, NULL);
#if NO_TOUCH_HACK
    _SYS_setVector(_SYS_CUBE_TILT, (void*)&OnCubeTilt, NULL);
#endif

    neighborEventHandler = NULL;

    currentPuzzle = NULL;
    previousPuzzle = NULL;

    timeStep.next();
    dt = timeStep.delta();

    //TODO		saveData.Load();

    StingController::Run();

    GameState nextState =
    #if SKIP_INTRO_TUTORIAL
        GameState_Menu;
    #else
        saveData.HasCompletedTutorial() ? GameState_Menu : GameState_Tutorial;
    #endif


    while(1)
    {
        switch(nextState)
        {           
        case GameState_Puzzle:
            nextState = PuzzleController::Run();
            break;

        case GameState_Menu:
            Skins::SetSkin(Skins::SkinType_Default);
            nextState = MenuController::Run();
            break;

        case GameState_Interstitial:
            nextState = InterstitialController::Run();
            break;

        case GameState_Tutorial:
            Skins::SetSkin(Skins::SkinType_Default);
            nextState = TutorialController::Run();
            break;

        case GameState_Victory:            
            nextState = VictoryController::Run();
            break;

        case GameState_IsOver:
            nextState = IsGameOver();
            break;

        case GameState_Advance:
            nextState = Advance();
            break;
        }
    }
}

void UpdateDt()
{
    timeStep.next();
    dt = timeStep.delta();
}

void Wait(float delay)
{    
    PaintCubeViews();
    System::paint();
    System::finish();

    SystemTime t = SystemTime::now();

    t += delay;
    do {
        System::yield();
        UpdateDt();
    } while(SystemTime::now() < t);
}

bool IsPlayingRandom()
{
    return currentPuzzle == NULL;
}

GameState Advance()
{
    delete previousPuzzle;
    previousPuzzle = currentPuzzle;
    if (Game::currentPuzzle == NULL)
    {
        randomPuzzleCount++;
        if(randomPuzzleCount >= RandomPuzzlesPerChapter)
        {
            return GameState_Victory;
        }
        else
        {
            return GameState_Puzzle;
        }
    }

    currentPuzzle->SaveAsSolved();
    int chapter = currentPuzzle->chapterIndex;
    int puzzle = currentPuzzle->puzzleIndex;
    bool success;
    success = currentPuzzle->GetNext(NUM_CUBES, &chapter, &puzzle);
    if (!success)
    {
        saveData.AddSolvedPuzzle(currentPuzzle->chapterIndex, currentPuzzle->puzzleIndex);
        delete Game::currentPuzzle;
        Game::currentPuzzle = NULL;
        return GameState_Victory;
    }
    else if (chapter == Game::currentPuzzle->chapterIndex)
    {
        delete Game::currentPuzzle;
        Game::currentPuzzle = Database::GetPuzzleInChapter(chapter, puzzle);
        return GameState_Puzzle;
    }
    else
    {
        saveData.AddSolvedChapter(Game::currentPuzzle->chapterIndex);
        delete Game::currentPuzzle;
        Game::currentPuzzle = Database::GetPuzzleInChapter(chapter, puzzle);
        return GameState_Victory;
    }
}

GameState IsGameOver()
{
    return currentPuzzle == NULL ? GameState_Menu : GameState_Interstitial;
}


}

}
