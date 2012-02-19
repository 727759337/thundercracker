#include "SaveData.h"
#include "Game.h"

namespace TotalsGame 
{
    SaveData::SaveData()
    {
        numSolvedGuids = 0;
    }
    
    void SaveData::AddSolved(const Guid &guid)
    {
        if(!IsSolved(guid))
        {
            solvedGuids[numSolvedGuids] = guid;
            numSolvedGuids++;
        }
    }
    
    void SaveData::Reset()
    {
        Game::GetInstance().currentPuzzle = NULL;
        Game::GetInstance().previousPuzzle = NULL;
        numSolvedGuids = 0;
        Save();
    }

    void SaveData::Save() 
    {
        //TODO
    }
    
    bool SaveData::IsSolved(const Guid &guid)
    {
        for(int i = 0; i < numSolvedGuids; i++)
        {
            if(solvedGuids[i] == guid)
                return true;
        }
        return false;
        
    }
    

}

