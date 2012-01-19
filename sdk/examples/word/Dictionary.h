#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "CubeStateMachine.h"

const unsigned MAX_OLD_WORDS = 16; // as determined by offline check

class Dictionary
{
public:
    Dictionary();

    static bool pickWord(char* buffer);
    static bool isWord(const char* string);
    static bool isOldWord(const char* word);

    static void sOnEvent(unsigned eventID, const EventData& data);

private:
    static char sOldWords[MAX_OLD_WORDS][MAX_LETTERS_PER_WORD + 1];
    static unsigned sNumOldWords;
    static unsigned sRandSeed;
    static unsigned sRound;
    static unsigned sPickIndex;
};

#endif // DICTIONARY_H
