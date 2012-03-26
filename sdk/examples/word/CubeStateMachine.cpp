#include "CubeStateMachine.h"
#include "EventID.h"
#include "EventData.h"
#include "Dictionary.h"
#include "GameStateMachine.h"
#include "config.h"
#include "WordGame.h"
#include "assets.gen.h"

CubeStateMachine::CubeStateMachine() :
        StateMachine(0), mNumLetters(0), mPuzzlePieceIndex(0), mIdleTime(0.f),
        mNewHint(false), mPainting(false), mBG0Panning(0.f),
        mBG0TargetPanning(0.f), mBG0PanningLocked(true), mLettersStart(0),
        mLettersStartOld(0), mImageIndex(ImageIndex_ConnectedWord), mCube(0)
{
    mLetters[0] = '\0';
    for (unsigned i = 0; i < arraysize(mAnimTypes); ++i)
    {
        mAnimTypes[i] = AnimType_None;
    }

    for (unsigned i = 0; i < arraysize(mAnimTimes); ++i)
    {
        mAnimTimes[i] = 0.f;
    }
}

void CubeStateMachine::setCube(Cube& cube)
{
    mCube = &cube;
    for (unsigned i = 0; i < getNumStates(); ++i)
    {
        CubeState& state = (CubeState&)getState(i);
        state.setStateMachine(*this);
    }

    for (unsigned i = 0; i < arraysize(mTilePositions); ++i)
    {
        mTilePositions[i].x = 2 + i * 6;
        mTilePositions[i].y = 2;
    }
}

Cube& CubeStateMachine::getCube()
{
    ASSERT(mCube != 0);
    return *mCube;
}

unsigned CubeStateMachine::onEvent(unsigned eventID, const EventData& data)
{
    switch (eventID)
    {
    case EventID_Paint:
    case EventID_ClockTick:
        paint();
        break;

    case EventID_Tilt:
        switch (mAnimTypes[CubeAnim_Main])
        {
        default:
        case AnimType_SlideL:
        case AnimType_SlideR:
            break;

        case AnimType_NotWord:
        case AnimType_OldWord:
        case AnimType_NewWord:

            if (data.mInput.mCubeID == getCube().id())
            {
                switch (GameStateMachine::getCurrentMaxLettersPerCube())
                {
                case 2:
                case 3:
                    if (!mBG0PanningLocked)
                    {
                        const float BG0_PANNING_WRAP = 144.f;

                        _SYSTiltState state = _SYS_getTilt(getCube().id());
                        if (state.x != 1)
                        {

                            mBG0TargetPanning -=
                                    BG0_PANNING_WRAP/GameStateMachine::getCurrentMaxLettersPerCube() * (state.x - 1);
                            while (mBG0TargetPanning < 0.f)
                            {
                                mBG0TargetPanning += BG0_PANNING_WRAP;
                                mBG0Panning += BG0_PANNING_WRAP;
                            }

                            VidMode_BG0_SPR_BG1 vid(getCube().vbuf);
                            setPanning(vid, mBG0Panning);
                            if (state.x < 1)
                            {
                                queueAnim(AnimType_SlideL);//, vid); // FIXME
                            }
                            else
                            {
                                queueAnim(AnimType_SlideR);//, vid); // FIXME
                            }

                            unsigned newStart = mLettersStart + state.x - 1 + GameStateMachine::getCurrentMaxLettersPerCube();
                            newStart = (newStart % GameStateMachine::getCurrentMaxLettersPerCube());
                            setLettersStart(newStart);
                            // letters are unavailable until anim finishes, but
                            // need to break word now
                            WordGame::instance()->onEvent(EventID_LetterOrderChange, EventData());
                        }

                    }
                    break;

                default:
                    break;
                }
            }
            if (data.mInput.mCubeID == mCube->id())
            {
                mIdleTime = 0.f;
            }
            break;
        }
        break;

    case EventID_Shake:
        if (data.mInput.mCubeID == mCube->id())
        {
            mIdleTime = 0.f;
        }
        break;

    case EventID_GameStateChanged:
        mBG0PanningLocked = (data.mGameStateChanged.mNewStateIndex != GameStateIndex_PlayScored);
        mBG0TargetPanning = 0.f;
        {
            VidMode_BG0_SPR_BG1 vid(getCube().vbuf);
            setPanning(vid, 0.f);
            vid.BG1_setPanning(Vec2(0, 0));
        }
        mIdleTime = 0.f;
        switch (data.mGameStateChanged.mNewStateIndex)
        {
        case GameStateIndex_EndOfRoundScored:
            queueAnim(AnimType_EndofRound);
            break;

        case GameStateIndex_ShuffleScored:
            queueAnim(AnimType_Shuffle);
            break;

        case GameStateIndex_StoryCityProgression:
            queueAnim(AnimType_CityProgression);
            break;
        }
        break;

    case EventID_EnterState:
        queueNextAnim();//vid, bg1, params);
        mIdleTime = 0.f;
        paint();
        WordGame::instance()->setNeedsPaintSync();
        break;

    case EventID_AddNeighbor:
    case EventID_RemoveNeighbor:
    case EventID_LetterOrderChange:
        switch (mAnimTypes[CubeAnim_Hint])
        {
        default:
            break;

        case AnimType_HintWindUpSlide:
            queueAnim(AnimType_None, CubeAnim_Hint);
            break;

        case AnimType_HintSlideL:
        case AnimType_HintSlideR:
            {
                unsigned newStart = 0;
                unsigned tiltDir = 0;
                if (!calcHintTiltDirection(newStart, tiltDir))
                {
                    queueAnim(AnimType_None, CubeAnim_Hint);
                }
            }
            break;
        }

        switch (mAnimTypes[CubeAnim_Main])
        {
        default:
        case AnimType_NewWord: // see ::update (wait for min display time)
            break;

        case AnimType_NotWord:
        case AnimType_OldWord:
            {
                bool isOldWord = false;
                if (canBeginWord())
                {
                    char wordBuffer[MAX_LETTERS_PER_WORD + 1];
                    EventData wordFoundData;
                    if (beginsWord(isOldWord, wordBuffer, wordFoundData.mWordFound.mBonus))
                    {
                        wordFoundData.mWordFound.mCubeIDStart = getCube().id();
                        wordFoundData.mWordFound.mWord = wordBuffer;
                        if (isOldWord)
                        {
                            GameStateMachine::sOnEvent(EventID_OldWordFound, wordFoundData);                            
                            queueAnim(AnimType_OldWord);
                        }
                        else
                        {
                            GameStateMachine::sOnEvent(EventID_NewWordFound, wordFoundData);
                            queueAnim(AnimType_NewWord);
                        }
                    }
                    else
                    {
                        EventData wordBrokenData;
                        wordBrokenData.mWordBroken.mCubeIDStart = getCube().id();
                        GameStateMachine::sOnEvent(EventID_WordBroken, wordBrokenData);
                        queueAnim(AnimType_NotWord);
                    }
                }
                else if (hasNoNeighbors())
                {
                    queueAnim(AnimType_NotWord);
                }
            }
            break;
        }

        mIdleTime = 0.f;
        paint();
        break;

    case EventID_NewWordFound:
        {
            Cube& c = getCube();
            mImageIndex = ImageIndex_ConnectedWord;
            if (c.physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
                c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedLeftWord;
            }
            else if (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
                     c.physicalNeighborAt(SIDE_RIGHT) == CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedRightWord;
            }
        }

        switch (mAnimTypes[CubeAnim_Main])
        {
        case AnimType_NewWord:
            if (isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
            {
                resetStateTime();
            }
            break;

        default:
            if (!canBeginWord() &&
                 isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
            {
                queueAnim(AnimType_NewWord);
            }
            break;
        }
        paint();
        break;

    case EventID_OldWordFound:
        if (!canBeginWord() &&
             isConnectedToCubeOnSide(data.mWordFound.mCubeIDStart))
        {
            queueAnim(AnimType_OldWord);
        }
        break;

    case EventID_WordBroken:
        switch (getAnim())
        {
        case AnimType_OldWord:
        case AnimType_NewWord:
            if (!canBeginWord() &&
                isConnectedToCubeOnSide(data.mWordBroken.mCubeIDStart))
            {
                queueAnim(AnimType_NotWord);
            }
            break;

        default:
            break;
        }
        break;

    case EventID_NewAnagram:
        {
            unsigned cubeIndex = (getCube().id() - CUBE_ID_BASE);
            setLettersStart(0);
            for (unsigned i = 0; i < arraysize(mLetters); ++i)
            {
                mLetters[i] = '\0';
            }

            // TODO multiple letters: variable
            for (unsigned i = 0; i < GameStateMachine::getCurrentMaxLettersPerCube(); ++i)
            {
                mLetters[i] =
                        data.mNewAnagram.mWord[cubeIndex * GameStateMachine::getCurrentMaxLettersPerCube() + i];
            }
            mNumLetters = GameStateMachine::getCurrentMaxLettersPerCube(); // FIXME this var name is misleading
            mPuzzlePieceIndex = data.mNewAnagram.mPuzzlePieceIndexes[cubeIndex];
            // TODO substrings of length 1 to 3
            paint();
        }
        break;

    case EventID_HintSolutionUpdated:
        {
            _SYS_strlcpy(mHintSolution,
                         data.mHintSolutionUpdate.mHintSolution[mPuzzlePieceIndex],
                         sizeof(mHintSolution));
        }
        break;

    case EventID_SpendHint:
        switch (mAnimTypes[CubeAnim_Hint])
        {
        case AnimType_HintWindUpSlide:
            queueAnim(AnimType_None, CubeAnim_Hint);
            break;

        default:
            break;
        }
        break;
    }
    return StateMachine::onEvent(eventID, data);
}

unsigned CubeStateMachine::findNumLetters(char *string)
{
    if (string == 0)
    {
        return 0;
    }

    unsigned count = 0;
    for (unsigned i=0; i<GameStateMachine::getCurrentMaxLettersPerCube(); ++i)
    {
        if (string[i] < 'A' || string[i] > 'Z')
        {
            return count;
        }
        ++count;
    }
    return count;
}

unsigned CubeStateMachine::getLetters(char *buffer, bool forPaint)
{
    if (mNumLetters <= 0)
    {
        return 0;
    }

    unsigned start = mLettersStart;
    switch (mAnimTypes[CubeAnim_Main])
    {
    case AnimType_SlideL:
    case AnimType_SlideR:
        if (!forPaint)
        {
            return 0;
        }
        start = mLettersStartOld;
        // fall through
    default:
        if (start == 0)
        {
            _SYS_strlcpy(buffer,
                         mLetters,
                         GameStateMachine::getCurrentMaxLettersPerCube() + 1);
        }
        else
        {
            //DEBUG_LOG(("letters start: %d\n", mLettersStart));
            // copy from the (offset) start to the end of the letters
            _SYS_strlcpy(buffer,
                         &mLetters[start],
                         GameStateMachine::getCurrentMaxLettersPerCube() + 1 - start);
            // fill in the rest of the buffer with the substring of the letters
            // that came after the end of the mLetters buffer, and zero terminate
            _SYS_strlcat(buffer,
                         mLetters,
                         GameStateMachine::getCurrentMaxLettersPerCube() + 1);
        }
        break;

    //default:
      //  _SYS_strlcpy(buffer, mLetters, GameStateMachine::getCurrentMaxLettersPerCube() + 1);
        //break;
    }

    return _SYS_strnlen(buffer, GameStateMachine::getCurrentMaxLettersPerCube());
}

void CubeStateMachine::queueAnim(AnimType anim, CubeAnim cubeAnim)
{
    DEBUG_LOG(("queue anim cube ID: %d,\tAnimType: %d,\tCubeAnim:\t%d\n",
               getCube().id(), anim, cubeAnim));

    mAnimTypes[cubeAnim] = anim;
    mAnimTimes[cubeAnim] = 0.f;
    WordGame::instance()->setNeedsPaintSync();

    // FIXME params aren't really sent through right now: animPaint(anim, vid, bg1, mAnimTime, params);

    switch (anim)
    {
    default:
        break;

    case AnimType_NewWord:
        {
            Cube& c = getCube();
            mImageIndex = ImageIndex_ConnectedWord;
            if (c.physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
                c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedLeftWord;
            }
            else if (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
                     c.physicalNeighborAt(SIDE_RIGHT) == CUBE_ID_UNDEFINED)
            {
                mImageIndex = ImageIndex_ConnectedRightWord;
            }

            // setup sprite params
            for (unsigned i=0; i<arraysize(mSpriteParams.mPositions); ++i)
            {
                calcSpriteParams(i);
            }
        }
        //WordGame::instance()->setNeedsPaintSync();
        break;
    }
}

void CubeStateMachine::updateSpriteParams(float dt)
{
    for (unsigned i=0; i<arraysize(mSpriteParams.mPositions); ++i)
    {
        // eased approach
        const Float2 &v =
                (mSpriteParams.mEndPositions[i] - mSpriteParams.mPositions[i]);
        if (v.len2() < 2.f)
        {
            calcSpriteParams(i);
        }
        else if (mSpriteParams.mStartDelay[i] <= 0.f)
        {
            mSpriteParams.mPositions[i] += 6.f * dt * v;
        }
        else
        {
            mSpriteParams.mStartDelay[i] -= dt;
        }
    }
}

void CubeStateMachine::calcSpriteParams(unsigned i)
{
    switch (getAnim())
    {
    default:
    case AnimType_NewWord:
        {
            mSpriteParams.mPositions[i].x = 56.f;
            mSpriteParams.mPositions[i].y = 56.f;
            float angle = i * M_PI_4;
            mSpriteParams.mEndPositions[i].setPolar(WordGame::random.uniform(angle * .75f,
                                                                             angle * 1.25f),
                                                    WordGame::random.uniform(32.f, 52.f));
            mSpriteParams.mEndPositions[i] += Vec2(56.f, 56.f);
            mSpriteParams.mStartDelay[i] = WordGame::random.random() * 0.5f;
        }
        break;
    }
}

void CubeStateMachine::queueNextAnim(CubeAnim cubeAnim)
{
    AnimType oldAnim = mAnimTypes[cubeAnim];
    AnimType anim = getNextAnim(cubeAnim);
    queueAnim(anim, cubeAnim);//, vid, bg1, params);
    if (anim != oldAnim)
    {
        switch (oldAnim)
        {
        case AnimType_SlideL:
        case AnimType_SlideR:
            WordGame::instance()->onEvent(EventID_LetterOrderChange, EventData());
            break;

        default:
            break;
        }

        switch (anim)
        {
        case AnimType_HintSlideL:
        case AnimType_HintSlideR:
            WordGame::instance()->onEvent(EventID_SpendHint, EventData());
            break;

        default:
            break;
        }
    }
}

void CubeStateMachine::updateAnim(VidMode_BG0_SPR_BG1 &vid,
                                  BG1Helper *bg1,
                                  AnimParams *params)
{
    for (unsigned i = 0; i < NumCubeAnims; ++i)
    {
        if (params)
        {
            params->mBorders = (i == CubeAnim_Main); // FIXME fold border code into anim
        }
        if (mAnimTypes[i] != AnimType_None &&
            !animPaint(mAnimTypes[i], vid, bg1, mAnimTimes[i], params))
        {
            queueNextAnim((CubeAnim)i);//, vid, bg1, params);
        }
    }
}

bool CubeStateMachine::canBeginWord()
{
    return (mNumLetters > 0 &&
            mCube->physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
            mCube->physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED);
}

AnimType CubeStateMachine::getNextAnim(CubeAnim cubeAnim) const
{
    switch (mAnimTypes[(int) cubeAnim])
    {
    default:
        return (cubeAnim == CubeAnim_Main) ? AnimType_NotWord : mAnimTypes[(int) cubeAnim];

    case AnimType_HintSlideL:
    case AnimType_HintSlideR:
        return AnimType_None;// TODO HintBarDisappear;

    case AnimType_HintWindUpSlide:
        {
            unsigned nextStartIndex;
            unsigned tiltDir = 0;
            if (calcHintTiltDirection(nextStartIndex, tiltDir))
            {
                switch (tiltDir)
                {
                default:
                    return AnimType_None;

                case 1:
                    return AnimType_HintSlideL;

                case 2:
                    return AnimType_HintSlideR;
                }
            }
        }
        return AnimType_None;
    }
}

bool CubeStateMachine::beginsWord(bool& isOld, char* wordBuffer, bool& isBonus)
{
    if (canBeginWord())
    {        
        wordBuffer[0] = '\0';
        CubeStateMachine* csm = this;
        bool neighborLetters = false;
        for (Cube::ID neighborID = csm->mCube->physicalNeighborAt(SIDE_RIGHT);
             csm && neighborID != CUBE_ID_UNDEFINED;
             neighborID = csm->mCube->physicalNeighborAt(SIDE_RIGHT),
             csm = GameStateMachine::findCSMFromID(neighborID))
        {
            if (csm->mNumLetters <= 0)
            {
                break;
            }
            char str[MAX_LETTERS_PER_CUBE + 1];
            if (!csm->getLetters(str, false))
            {
                return false;
            }
            _SYS_strlcat(wordBuffer, str, GameStateMachine::getCurrentMaxLettersPerWord() + 1);
            neighborLetters = true;
        }
        if (neighborLetters)
        {
            char trimmedWord[MAX_LETTERS_PER_WORD + 1];
            if (Dictionary::trim(wordBuffer, trimmedWord))
            {
                _SYS_strlcpy(wordBuffer, trimmedWord, sizeof trimmedWord);
                if (Dictionary::isWord(trimmedWord, isBonus))
                {
                    isOld = Dictionary::isOldWord(trimmedWord);
                    return true;
                }
            }
        }
    }
    return false;
}

unsigned CubeStateMachine::findRowLength()
{
    unsigned result = 1;
    for (Cube::Side side = SIDE_LEFT; side <= SIDE_RIGHT; side +=2)
    {
        CubeStateMachine* csm = this;
        for (Cube::ID neighborID = csm->mCube->physicalNeighborAt(side);
             csm && neighborID != CUBE_ID_UNDEFINED;
             neighborID = csm->mCube->physicalNeighborAt(side),
             csm = GameStateMachine::findCSMFromID(neighborID))
        {
            if (csm != this)
            {
                ++result;
            }
        }
    }

    return result;
}

bool CubeStateMachine::hasNoNeighbors() const
{
    for (Cube::Side side = 0; side < NUM_SIDES; ++side)
    {
        if (mCube->physicalNeighborAt(side) != CUBE_ID_UNDEFINED)
        {
            return false;
        }
    }
    return true;
}

bool CubeStateMachine::isConnectedToCubeOnSide(Cube::ID cubeIDStart,
                                               Cube::Side side)
{
    if (mCube->id() == cubeIDStart)
    {
        return true;
    }

    CubeStateMachine* csm = this;
    for (Cube::ID neighborID = csm->mCube->physicalNeighborAt(side);
         csm && neighborID != CUBE_ID_UNDEFINED;
         neighborID = csm->mCube->physicalNeighborAt(side),
         csm = GameStateMachine::findCSMFromID(neighborID))
    {
        // only check the left most letter, as it it the one that
        // determines the entire word state
        if (neighborID == cubeIDStart)
        {
            return true;
        }
    }
    return false;
}

State& CubeStateMachine::getState(unsigned index)
{
    ASSERT(index < getNumStates());
    switch (index)
    {
    case CubeStateIndex_Title:
        return mTitleState;

    case CubeStateIndex_TitleExit:
        return mTitleExitState;

    default:
        ASSERT(0);
        // fall through
    case CubeStateIndex_NotWordScored:
        return mNotWordScoredState;

    case CubeStateIndex_NewWordScored:
        return mNewWordScoredState;

    case CubeStateIndex_OldWordScored:
        return mOldWordScoredState;

    case CubeStateIndex_StartOfRoundScored:
        return mStartOfRoundScoredState;

    case CubeStateIndex_EndOfRoundScored:
        return mEndOfRoundScoredState;

    case CubeStateIndex_ShuffleScored:
        return mShuffleScoredState;
    }
}

unsigned CubeStateMachine::getNumStates() const
{
    return CubeStateIndex_NumStates;
}

void CubeStateMachine::update(float dt)
{
    mIdleTime += dt;
    for (unsigned i = 0; i < NumCubeAnims; ++i)
    {
        if (mAnimTypes[i] != AnimType_None)
        {
            mAnimTimes[i] += dt;
        }
    }

    StateMachine::update(dt);
    if ((int)mBG0Panning != (int)mBG0TargetPanning)
    {
        mBG0Panning += (mBG0TargetPanning - mBG0Panning) * dt * 7.5f;
        VidMode_BG0_SPR_BG1 vid(getCube().vbuf);
        setPanning(vid, mBG0Panning);
    }
    switch (mAnimTypes[CubeAnim_Main])
    {
    default:
        break;

    case AnimType_NewWord:
        updateSpriteParams(dt);

        if (mAnimTimes[CubeAnim_Main] <= 1.5f)
        {
            // do nothing
        }
        else if (GameStateMachine::getNumAnagramsLeft() <= 0)
        {
            queueAnim(AnimType_Shuffle);
        }
        else
        {
            bool isOldWord = false;
            if (canBeginWord())
            {
                char wordBuffer[MAX_LETTERS_PER_WORD + 1];
                EventData wordFoundData;
                if (beginsWord(isOldWord, wordBuffer, wordFoundData.mWordFound.mBonus))
                {
                    wordFoundData.mWordFound.mCubeIDStart = getCube().id();
                    wordFoundData.mWordFound.mWord = wordBuffer;

                    if (isOldWord)
                    {
                        GameStateMachine::sOnEvent(EventID_OldWordFound, wordFoundData);
                        queueAnim(AnimType_OldWord);
                    }
                    else
                    {
                        GameStateMachine::sOnEvent(EventID_NewWordFound, wordFoundData);
                        queueAnim(AnimType_NewWord);
                    }
                }
                else
                {
                    EventData wordBrokenData;
                    wordBrokenData.mWordBroken.mCubeIDStart = getCube().id();
                    GameStateMachine::sOnEvent(EventID_WordBroken, wordBrokenData);
                    queueAnim(AnimType_NotWord);
                }
            }
            else if (hasNoNeighbors())
            {
                queueAnim(AnimType_NotWord);
            }
            else
            {
                queueAnim(AnimType_OldWord);
            }
        }
        break;
    }
}

void CubeStateMachine::setPanning(VidMode_BG0_SPR_BG1& vid, float panning)
{
    mBG0Panning = panning;
    int tilePanning = fmod(mBG0Panning, 144.f);
    tilePanning /= 8;
    // TODO clean this up
    int tileWidth = 12/GameStateMachine::getCurrentMaxLettersPerCube();
    for (unsigned i = 0; i < GameStateMachine::getCurrentMaxLettersPerCube(); ++i)
    {
        mTilePositions[i].x = 2 + tilePanning + i * tileWidth;
        mTilePositions[i].x = ((mTilePositions[i].x + tileWidth + 2) % (16 + 2 * tileWidth));
        mTilePositions[i].x -= tileWidth + 2;
    }
    //vid.BG0_setPanning(Vec2((int)mBG0Panning, 0.f));
}

void CubeStateMachine::paint()
{
    if (mPainting)
    {
        return;
    }
    mPainting = true;
    Cube& c = getCube();
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    vid.BG0_drawAsset(Vec2(0,0), TileBG);
    BG1Helper bg1(c);
    paintLetters(vid, bg1, Font1Letter, true);
    vid.BG0_setPanning(Vec2(0.f, 0.f));

    /* not word
    Cube& c = getCube();
    // FIXME vertical words
    bool neighbored =
            (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED ||
            c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED);
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();
    switch (GameStateMachine::getCurrentMaxLettersPerCube())
    {
    case 2:
        paintLetters(vid, Font2Letter, true);
        break;

    case 3:
        paintLetters(vid, Font3Letter, true);
        break;

    default:
        paintLetters(vid, Font1Letter, true);
        break;
    }

    if (neighbored)
    {
        paintBorder(vid, ImageIndex_Neighbored, true, false, true, false);
    }
    else
    {
        paintBorder(vid, ImageIndex_Teeth, false, true, false, true);
    }
*/

    /* old word
    Cube& c = getCube();
    VidMode_BG0_SPR_BG1 vid(c.vbuf);
    vid.init();

    switch (GameStateMachine::getCurrentMaxLettersPerCube())
    {
    case 2:
        paintLetters(vid, Font2Letter, true);
        break;

    case 3:
        paintLetters(vid, Font3Letter, true);
        break;

    default:
        paintLetters(vid, Font1Letter, true);
        break;
    }

    ImageIndex ii = ImageIndex_Connected;
    if (c.physicalNeighborAt(SIDE_LEFT) == CUBE_ID_UNDEFINED &&
        c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED)
    {
        ii = ImageIndex_ConnectedLeft;
    }
    else if (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED &&
             c.physicalNeighborAt(SIDE_RIGHT) == CUBE_ID_UNDEFINED)
    {
        ii = ImageIndex_ConnectedRight;
    }
    paintBorder(vid, ii, true, false, true, false);
    */
    bg1.Flush(); // TODO only flush if mask has changed recently

    mPainting = false;
}

void CubeStateMachine::paintScore(VidMode_BG0_SPR_BG1& vid,
                           ImageIndex teethImageIndex,
                           bool animate,
                           bool reverseAnim,
                           bool loopAnim,
                           bool paintTime,
                           float animStartTime)
{
#if (0)
    return;
    if (GameStateMachine::getCurrentMaxLettersPerCube() > 1)
    {
        paintTime = false;
    }

    if (teethImageIndex == ImageIndex_Teeth && reverseAnim)
    {
        // no blip when reversing teeth anim (use the same anim, but
        // diff transparency data, to save room)
        teethImageIndex = ImageIndex_Teeth_NoBlip;
    }
    const AssetImage* teethImages[NumImageIndexes] =
    {
        &TeethLoopWordTop,     // ImageIndex_Connected,
        &TeethNewWord2,      // ImageIndex_ConnectedWord,
        &TeethLoopWordLeftTop, // ImageIndex_ConnectedLeft,
        &TeethNewWord2Left,  // ImageIndex_ConnectedLeftWord,
        &TeethLoopWordRightTop,// ImageIndex_ConnectedRight,
        &TeethNewWord2Right, // ImageIndex_ConnectedRightWord,
        &TeethLoopNeighboredTop,// ImageIndex_Neighbored,
        &Teeth,             // ImageIndex_Teeth,
        &Teeth,             // ImageIndex_Teeth_NoBlip,
    };

    const AssetImage* teethNumberImages[] =
    {
        &TeethNewWord3,
        &TeethNewWord4,
        &TeethNewWord5,
    };

    const Int2 TEETH_NUM_POS = {4, 6};
    STATIC_ASSERT(arraysize(teethImages) == NumImageIndexes);
    ASSERT(teethImageIndex >= 0);
    ASSERT(teethImageIndex < (ImageIndex)arraysize(teethImages));
    const AssetImage* teeth = teethImages[teethImageIndex];
    const AssetImage* teethNumber = 0;

    unsigned teethNumberIndex = GameStateMachine::getNewWordLength() / GameStateMachine::getCurrentMaxLettersPerCube();
    if (teethNumberIndex > 2)
    {
        teethNumberIndex -= 3;
        switch (teethImageIndex)
        {
        case ImageIndex_ConnectedWord:
        case ImageIndex_ConnectedLeftWord:
        case ImageIndex_ConnectedRightWord:
            teethNumber = teethNumberImages[MIN(((unsigned)arraysize(teethNumberImages) - 1), teethNumberIndex)];
            break;

        default:
            break;
        }
    }
    unsigned frame = 0;
    unsigned secondsLeft = GameStateMachine::getSecondsLeft();

    if (animate)
    {
        float animTime =  (getTime() - animStartTime) / TEETH_ANIM_LENGTH;
        if (loopAnim)
        {
            animTime = fmod(animTime, 1.0f);
        }
        else
        {
            animTime = MIN(animTime, 1.f);
        }

        if (reverseAnim)
        {
            animTime = 1.f - animTime;
        }
        frame = (unsigned) (animTime * teeth->frames);
        frame = MIN(frame, teeth->frames - 1);

        //DEBUG_LOG(("shuffle: [c: %d] anim: %d, frame: %d, time: %f\n", getCube().id(), teethImageIndex, frame, GameStateMachine::getTime()));

    }
    else if (reverseAnim)
    {
        frame = teeth->frames - 1;
    }

    BG1Helper bg1(getCube());
    unsigned bg1Tiles = 2;
    for (unsigned int i = 0; i < 16; ++i) // rows
    {
        for (unsigned j=0; j < 16; ++j) // columns
        {

            Vec2 texCoord(j, i);
            switch (teethImageIndex)
            {
            case ImageIndex_Connected:
                if (i >= 14)
                {
                    teeth = &TeethLoopWordBottom;
                    texCoord.y = i - 14;
                }
                break;

            case ImageIndex_Neighbored:
                if (i >= 14)
                {
                    teeth = &TeethLoopNeighboredBottom;
                    texCoord.y = i - 14;
                }
                break;

            case ImageIndex_ConnectedLeft:
                if (i >= 14)
                {
                    teeth = &TeethLoopWordLeftBottom;
                    texCoord.y = i - 14;
                }
                else if (i >= 2)
                {
                    teeth = &TeethLoopWordLeftLeft;
                    texCoord.x = j % teeth->width;
                    texCoord.y = i - 2;
                }
                break;

            case ImageIndex_ConnectedRight:
                if (i >= 14)
                {
                    teeth = &TeethLoopWordRightBottom;
                    texCoord.y = i - 14;
                }
                else if (i >= 2)
                {
                    teeth = &TeethLoopWordRightRight;
                    texCoord.x = j % teeth->width;
                    texCoord.y = i - 2;
                }
                break;

            default:
                break;
            }

            switch (getTransparencyType(teethImageIndex, frame, j, i))
            {
            case TransparencyType_None:
                if (GameStateMachine::getCurrentMaxLettersPerCube() == 1 ||
                    (animate && (teethImageIndex == ImageIndex_ConnectedRightWord ||
                                teethImageIndex == ImageIndex_ConnectedLeftWord ||
                                teethImageIndex == ImageIndex_ConnectedWord ||
                                teethImageIndex == ImageIndex_Teeth ||
                                teethImageIndex == ImageIndex_Teeth_NoBlip)))
                {
                    // paint this opaque tile
                    // paint BG0
                    if (teethNumber &&
                        frame >= 2 && frame - 2 < teethNumber->frames &&
                        j >= ((unsigned) TEETH_NUM_POS.x) &&
                        j < teethNumber->width + ((unsigned) TEETH_NUM_POS.x) &&
                        i >= ((unsigned) TEETH_NUM_POS.y) &&
                        i < teethNumber->height + ((unsigned) TEETH_NUM_POS.y))
                    {
                        vid.BG0_drawPartialAsset(Vec2(j, i),
                                                 Vec2(j - TEETH_NUM_POS.x, i - TEETH_NUM_POS.y),
                                                 Vec2(1, 1),
                                                 *teethNumber,
                                                 frame - 2);
                    }
                    else
                    {
                        vid.BG0_drawPartialAsset(Vec2(j, i), texCoord, Vec2(1, 1), *teeth, frame);
                    }
                    break;
                }
                // else fall through

            case TransparencyType_Some:
                if (bg1Tiles < 144)
                {
                    if (teethNumber &&
                        frame >= 2 && frame - 2 < teethNumber->frames &&
                        j >= ((unsigned) TEETH_NUM_POS.x) &&
                        j < teethNumber->width + ((unsigned) TEETH_NUM_POS.x) &&
                        i >= ((unsigned) TEETH_NUM_POS.y) &&
                        i < teethNumber->height + ((unsigned) TEETH_NUM_POS.y))
                    {
                        bg1.DrawPartialAsset(Vec2(j, i),
                                             Vec2(j - TEETH_NUM_POS.x, i - TEETH_NUM_POS.y),
                                             Vec2(1, 1),
                                             *teethNumber,
                                             frame - 2);
                    }
                    else
                    {
                        bg1.DrawPartialAsset(Vec2(j, i), texCoord, Vec2(1, 1), *teeth, frame);
                    }
                    ++bg1Tiles;
                }
                break;

            default:
                ASSERT(getTransparencyType(teethImageIndex, frame, j, i) == TransparencyType_All);
                break;
            }
        }
    }

    if (paintTime)
    {
        int AnimType = -1;

        switch (secondsLeft)
        {
        case 30:
        case 20:
        case 10:
            AnimType = secondsLeft/10 + 2;
            break;

        case 3:
        case 2:
        case 1:
            AnimType = secondsLeft - 1;
            break;
        }

        // 1, 2, 3, 10, 20, 30
        float animLength[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

        // 1, 2, 3, 10, 20, 30
        const AssetImage* lowDigitAnim[6] =
            {&TeethClockPulse1, &TeethClockPulse2, &TeethClockPulse3,
             &TeethClockPulse0, &TeethClockPulse0, &TeethClockPulse0};

        const AssetImage* highDigitAnim[6] =
            {0, 0, 0,
             &TeethClockPulse1, &TeethClockPulse2, &TeethClockPulse3};

        frame = 0;
        float animTime =
                1.f - fmod(GameStateMachine::getSecondsLeftFloat(), 1.f);
        if (AnimType >= 0 && animTime < animLength[AnimType])
        {
            // normalize
            animTime /= animLength[AnimType];
            animTime = MIN(animTime, 1.f);
            frame = (unsigned) (animTime * lowDigitAnim[AnimType]->frames);
            frame = MIN(frame, lowDigitAnim[AnimType]->frames - 1);

            if (highDigitAnim[AnimType] > 0)
            {
                bg1.DrawAsset(Vec2(((3 - 2 + 0) * 4 + 1), 14),
                              *highDigitAnim[AnimType],
                              frame);
            }
            bg1.DrawAsset(Vec2(((3 - 2 + 1) * 4 + 1), 14),
                          *lowDigitAnim[AnimType],
                          frame);
        }
        else
        {
            String<5> string;
            string << secondsLeft;
            unsigned len = string.size();

            for (unsigned i = 0; i < len; ++i)
            {
                frame = string[i] - '0';
                bg1.DrawAsset(Vec2(((3 - len + i) * 4 + 1), 14),
                              FontTeeth,
                              frame);
            }
        }
    }

    // TODO merge in 2 ltr cube proto code
    if (GameStateMachine::getCurrentMaxLettersPerCube() > 1 &&
        !(animate &&
            (teethImageIndex == ImageIndex_ConnectedRightWord ||
            teethImageIndex == ImageIndex_ConnectedLeftWord ||
            teethImageIndex == ImageIndex_ConnectedWord ||
            teethImageIndex == ImageIndex_Teeth ||
            teethImageIndex == ImageIndex_Teeth_NoBlip)))
    {
        ASSERT(GameStateMachine::getNumAnagramsLeft() < 100);
        unsigned tensDigit = GameStateMachine::getNumAnagramsLeft() / 10;
        if (tensDigit)
        {
            bg1.DrawAsset(Vec2(7,11), FontSmall, tensDigit);
        }
        bg1.DrawAsset(Vec2(8,11), FontSmall, GameStateMachine::getNumAnagramsLeft() % 10);

        if (GameStateMachine::getNumBonusAnagramsLeft())
        {
            tensDigit = GameStateMachine::getNumBonusAnagramsLeft() / 10;
            if (tensDigit)
            {
                bg1.DrawAsset(Vec2(1,11), FontBonus, tensDigit);
            }
            bg1.DrawAsset(Vec2(2,11), FontBonus, GameStateMachine::getNumBonusAnagramsLeft() % 10);
        }
    }

    bg1.Flush(); // TODO only flush if mask has changed recently
    //WordGame::instance()->setNeedsPaintSync();
#endif // (0)
}

void CubeStateMachine::paintLetters(VidMode_BG0_SPR_BG1 &vid,
                                    BG1Helper &bg1,
                                    const AssetImage &fontREMOVE,
                                    bool paintSprites)
{
    const static AssetImage* fonts[] =
    {
        &Font1Letter, &Font2Letter, &Font3Letter,
    };
    const AssetImage& font = *fonts[GameStateMachine::getCurrentMaxLettersPerCube() - 1];

    switch (mAnimTypes[CubeAnim_Hint])
    {
    case AnimType_HintWindUpSlide:
    case AnimType_HintSlideL:
    case AnimType_HintSlideR:
    case AnimType_HintNeighborL:
    case AnimType_HintNeighborR:
        break;

    default:
        vid.hideSprite(0); // TODO sprite IDs
        break;
    }

    switch (mAnimTypes[CubeAnim_Main])
    {
    case AnimType_NewWord:
        break;

    default:
        for (unsigned i=1; i<8; ++i)
        {
            vid.hideSprite(i);
        }
        break;
    }

    Cube &c = getCube();
    AnimParams params;
    getAnimParams(&params);
    updateAnim(vid, &bg1, &params);

#if (0)
    switch (GameStateMachine::getCurrentMaxLettersPerCube())
    {
    case 2:
        {
        }
      break;

    case 3:
        /* TODO remove
vid.BG0_drawAsset(Vec2(0,0), ScreenOff);
        vid.BG0_drawPartialAsset(Vec2(17, 0),
                                 Vec2(0, 0),
                                 Vec2(1, 16),
                                 ScreenOff);
        vid.BG0_drawPartialAsset(Vec2(16, 0),
                                 Vec2(0, 0),
                                 Vec2(1, 16),
                                 ScreenOff);
                                 */
        {
            unsigned frame = str[0] - (int)'A';

            if (frame < font.frames)
            {
                vid.BG0_drawAsset(Vec2(0,6), font, frame);
            }

            frame = str[1] - (int)'A';
            if (frame < font.frames)
            {
                vid.BG0_drawAsset(Vec2(6,6), font, frame);
            }

            frame = str[2] - (int)'A';
            if (frame < font.frames)
            {
                vid.BG0_drawAsset(Vec2(12,6), font, frame);
            }
        }
      break;

    default:
        vid.BG0_drawAsset(Vec2(0,0), TileBG);
        {
            unsigned frame = *str - (int)'A';

            if (frame < font.frames)
            {
                vid.BG0_drawAsset(Vec2(1,3), font, frame);

/*
                if (paintSprites)
                {
                                WordGame::random.uniform(BlinkDelayMin[personality],
                                                         BlinkDelayMax[personality]);
                        }
                        mEyeState = newEyeState;
                    }

                    switch (mEyeState)
                    {
                    case EyeState_Closed:
                        vid.setSpriteImage(LetterStateSpriteID_LeftEye, EyeLeftBlink.index);
                        vid.resizeSprite(LetterStateSpriteID_LeftEye, EyeLeft.width * 8, EyeLeft.height * 8);
                        vid.moveSprite(LetterStateSpriteID_LeftEye, ed.lx, ed.ly);
                }
                else
                {
                    WordGame::hideSprites(vid);
                }
                */
            }
        }
        break;
    }
#endif
}

void CubeStateMachine::paintScoreNumbers(BG1Helper &bg1, Int2 position, const char* string)
{
    const AssetImage& font = FontSmall;

    const unsigned MAX_SCORE_STRLEN = 7;
    const char* MAX_SCORE_STR = "9999999";

    unsigned len = _SYS_strnlen(string, MAX_SCORE_STRLEN + 1);

    if (len > MAX_SCORE_STRLEN)
    {
        string = MAX_SCORE_STR;
        len = MAX_SCORE_STRLEN;
    }
    position.x -= len - 1;

    for (; *string; ++string)
    {
        unsigned index = *string - '0';
        ASSERT(index < font.frames);
        position.x++;
        bg1.DrawAsset(position, font, index);
    }
}

bool CubeStateMachine::getAnimParams(AnimParams *params)
{
    ASSERT(params);
    Cube &c = getCube();
    params->mLetters[0] = '\0';
    params->mSpriteParams = 0;
    switch (mAnimTypes[CubeAnim_Main])
    {
    case AnimType_EndofRound:
    case AnimType_Shuffle:
    case AnimType_CityProgression:
        break;

    case AnimType_NewWord:
        params->mSpriteParams = &mSpriteParams;
        // fall through
    default:
        if (!getLetters(params->mLetters, true))
        {
            return false;
        }
        break;
    }

    params->mLeftNeighbor = (c.physicalNeighborAt(SIDE_LEFT) != CUBE_ID_UNDEFINED);
    params->mRightNeighbor = (c.physicalNeighborAt(SIDE_RIGHT) != CUBE_ID_UNDEFINED);
    params->mCubeID = getCube().id();
    params->mBorders = false; // FIXME roll into border rendering CubeAnim
    params->mBonus = false; // TODO
    return true;
}

void CubeStateMachine::setLettersStart(unsigned s)
{
    if (s != mLettersStart)
    {
        mLettersStartOld = mLettersStart;
        mLettersStart = s;
        // new state is ready to react to level order change
        WordGame::instance()->onEvent(EventID_LetterOrderChange, EventData());
    }
}

bool CubeStateMachine::canStartHint() const
{
    switch (mAnimTypes[CubeAnim_Hint])
    {
    case AnimType_HintWindUpSlide:
    case AnimType_HintSlideL:
    case AnimType_HintSlideR:
    case AnimType_HintNeighborL:
    case AnimType_HintNeighborR:
        return false;

    default:
        {
            return true;
        }
    }
}

bool CubeStateMachine::canUseHint() const
{
    unsigned newLettersStart, tiltDirection;
    return calcHintTiltDirection(newLettersStart, tiltDirection);
}

bool CubeStateMachine::calcHintTiltDirection(unsigned &newLettersStart,
                                             unsigned &tiltDirection) const
{
    unsigned maxLetters = GameStateMachine::getCurrentMaxLettersPerCube();
    bool allLettersSame = true;
    char letter = mLetters[0];
    for (unsigned j=0; j < maxLetters; ++j)
    {
        if (mLetters[j] != letter)
        {
            allLettersSame = false;
            break;
        }
    }

    if (allLettersSame)
    {
        return false;
    }

    GameStateMachine::sOnEvent(EventID_UpdateHintSolution, EventData());
    // now determine which way to slide, if any, to put in hint configuration
    bool allMatch = true;
    // for all possible values of a letter start offset
    for (newLettersStart=0; newLettersStart < maxLetters; ++newLettersStart)
    {
        allMatch = true;
        // compare all the letters, using the offset
        for (unsigned j=0; j < maxLetters; ++j)
        {
            if (mLetters[(j + newLettersStart) % arraysize(mLetters)] !=
                    mHintSolution[j])
            {
                allMatch = false;
                break;
            }
            if (allMatch)
            {
                break;
            }
        }
        if (allMatch)
        {
            break;
        }
    }

    tiltDirection = (mLettersStart + maxLetters - newLettersStart) % maxLetters;
    return allMatch && tiltDirection != 0;
}
