#ifndef EVENTDATA_H
#define EVENTDATA_H

#include <sifteo.h>
using namespace Sifteo;

union EventData
{
    EventData() {}
    struct
    {
        const char* mWord;
        int mOffLengthIndex;
    } mNewAnagram;

    struct
    {
        const char* mWord;
        Cube::ID mCubeIDStart;
    } mWordFound; // used for NewWordFound and OldWordFound

    struct
    {
        Cube::ID mCubeIDStart;
    } mWordBroken;

    struct
    {
        bool mFirst;
        unsigned mPreviousStateIndex;
    } mEnterState;    

    struct
    {
        unsigned mPreviousStateIndex;
        unsigned mNewStateIndex;
    } mGameStateChanged;
};

#endif // EVENTDATA_H
