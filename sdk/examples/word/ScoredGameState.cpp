#include "ScoredGameState.h"
#include "EventID.h"
#include "EventData.h"
#include "Dictionary.h"
#include "GameStateMachine.h"
#include "assets.gen.h"
#include "WordGame.h"

unsigned ScoredGameState::update(float dt, float stateTime)
{
    if (GameStateMachine::getSecondsLeft() <= 0)
    {
        return GameStateIndex_EndOfRoundScored;
    }
    else
    {
        if (GameStateMachine::getNumAnagramsRemaining() <= 0 &&
            GameStateMachine::getNumCubesInState(CubeStateIndex_NewWordScored) <= 0)
        {
            // wait for all the cube states to exit the new word state
            // then shuffle
            WordGame::playAudio(shake, AudioChannelIndex_Shake);
            return GameStateIndex_ShuffleScored;        }

        return GameStateIndex_PlayScored;
    }
}

unsigned ScoredGameState::onEvent(unsigned eventID, const EventData& data)
{
    onAudioEvent(eventID, data);
    switch (eventID)
    {
    case EventID_Input:
        if (GameStateMachine::getAnagramCooldown() <= .0f &&
            GameStateMachine::getSecondsLeft() > 3)
        {
            WordGame::playAudio(shake, AudioChannelIndex_Shake);
            return GameStateIndex_ShuffleScored;
        }
        break;

    default:
        break;
    }
    return GameStateIndex_PlayScored;
}

void ScoredGameState::onAudioEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    case EventID_AddNeighbor:
        WordGame::playAudio(neighbor, AudioChannelIndex_Neighbor, LoopOnce, AudioPriority_Low);
        break;

    case EventID_NewWordFound:
//        WordGame::playAudio(fireball_laugh, AudioChannelIndex_Score);
        switch (_SYS_strnlen(data.mWordFound.mWord, GameStateMachine::getCurrentMaxLettersPerWord()) / GameStateMachine::getCurrentMaxLettersPerCube())
        {
        case 2:
            WordGame::playAudio(fanfare_fire_laugh_01, AudioChannelIndex_Score);
            break;

        case 3:
            WordGame::playAudio(fanfare_fire_laugh_02, AudioChannelIndex_Score);
            break;

        case 4:
            WordGame::playAudio(fanfare_fire_laugh_03, AudioChannelIndex_Score);
            break;

        case 5:
            WordGame::playAudio(fanfare_fire_laugh_04, AudioChannelIndex_Score);
            break;

        default:
            WordGame::playAudio(fanfare_fire_laugh_04, AudioChannelIndex_Score);
            break;
        }
        break;

    case EventID_OldWordFound:
        WordGame::playAudio(lip_snort, AudioChannelIndex_Score, LoopOnce, AudioPriority_Low);
        break;

    case EventID_ClockTick:
        switch (GameStateMachine::getSecondsLeft())
        {
        case 30:
//            WordGame::playAudio(bonus, AudioChannelIndex_Time);

            WordGame::playAudio(timer_30sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;

        case 20:
//            WordGame::playAudio(pause_on, AudioChannelIndex_Time);
            WordGame::playAudio(timer_20sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;

        case 10:
//            WordGame::playAudio(pause_off, AudioChannelIndex_Time);
            WordGame::playAudio(timer_10sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;

        case 3:
            WordGame::playAudio(timer_3sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;

        case 2:
            WordGame::playAudio(timer_2sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;

        case 1:
            WordGame::playAudio(timer_1sec, AudioChannelIndex_Time, LoopOnce, AudioPriority_High);
            break;
        }
        break;

    default:
        break;
    }
}

void ScoredGameState::createNewAnagram()
{
    // make a new anagram of letters halfway through the shuffle animation
    EventData data;
    Dictionary::pickWord(data.mNewAnagram.mWord, data.mNewAnagram.mNumAnagrams);
    // scramble the string (random permutation)
    char scrambled[MAX_LETTERS_PER_WORD + 1];
    _SYS_memset8((uint8_t*)scrambled, 0, sizeof(scrambled));
    switch (GameStateMachine::getCurrentMaxLettersPerCube())
    {
    default:
        for (unsigned i = 0; i < GameStateMachine::getCurrentMaxLettersPerWord(); ++i)
        {
            if (data.mNewAnagram.mWord[i] == '\0')
            {
                break;
            }

            // for each letter, place it randomly in the scrambled array
        for (unsigned j = WordGame::random.randrange(GameStateMachine::getCurrentMaxLettersPerWord());
                 true;
                 j = (j + 1) % GameStateMachine::getCurrentMaxLettersPerWord())
            {
                if (scrambled[j] == '\0')
                {
                    scrambled[j] = data.mNewAnagram.mWord[i];
                    break;
                }
            }
        }
        break;

    case 2:
        {
            // first scramble the cube to word fragments mapping
            int normal[NUM_CUBES];
            for (int i = 0; i < (int)arraysize(normal); ++i)
            {
                normal[i] = i;
            }

            int scrambledCubes[NUM_CUBES];
            for (int i = 0; i < (int)arraysize(scrambledCubes); ++i)
            {
                for (unsigned j = WordGame::random.randrange(NUM_CUBES);
                     true;
                     j = ((j + 1) % NUM_CUBES))
                {
                    if (normal[j] >= 0)
                    {
                        scrambledCubes[i] = normal[j];
                        normal[j] = -1;
                        break;
                    }
                }
            }

            for (unsigned ci = 0; ci < NUM_CUBES; ++ci)
            {
                // for each letter, place it randomly in the scrambled array
                unsigned cubeIndex = (unsigned)scrambledCubes[ci];
                unsigned ltrStartSrc = cubeIndex * GameStateMachine::getCurrentMaxLettersPerCube();
                unsigned ltrStartDest = ci * GameStateMachine::getCurrentMaxLettersPerCube();
                for (unsigned i = ltrStartSrc; i < GameStateMachine::getCurrentMaxLettersPerCube() + ltrStartSrc; ++i)
                {
                    if (data.mNewAnagram.mWord[i] == '\0')
                    {
                        break;
                    }

                    for (unsigned j = WordGame::random.randrange(GameStateMachine::getCurrentMaxLettersPerCube()) + ltrStartDest;
                         true;
                         j = ((j + 1) % (GameStateMachine::getCurrentMaxLettersPerCube() + ltrStartDest)))
                    {
                        if (scrambled[j] == '\0')
                        {
                            scrambled[j] = data.mNewAnagram.mWord[i];
                            break;
                        }
                    }
                }
            }
        }
        break;
    }
    LOG(("scrambled %s to %s\n", data.mNewAnagram.mWord, scrambled));
    ASSERT(_SYS_strnlen(scrambled, GameStateMachine::getCurrentMaxLettersPerWord() + 1) == _SYS_strnlen(data.mNewAnagram.mWord, GameStateMachine::getCurrentMaxLettersPerWord() + 1));
    _SYS_strlcpy(data.mNewAnagram.mWord, scrambled, sizeof data.mNewAnagram.mWord);
    unsigned wordLen = _SYS_strnlen(data.mNewAnagram.mWord, sizeof data.mNewAnagram.mWord);
    unsigned numCubes = GameStateMachine::GetNumCubes();
    if ((wordLen % numCubes) == 0)
    {
        // all cubes have word fragments of the same length
        data.mNewAnagram.mOffLengthIndex = -1;
    }
    else
    {
        // TODO odd number of multiple letters per cube

        // pick a cube index randomly to have the off length
        // word fragment
        //data.mNewAnagram.mOddIndex = ;
    }
    GameStateMachine::sOnEvent(EventID_NewAnagram, data);
}


