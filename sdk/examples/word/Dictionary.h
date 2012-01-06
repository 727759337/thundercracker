#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "CubeStateMachine.h"

const unsigned MAX_OLD_WORDS = 32; // as determined by offline check

class Dictionary
{
public:
    Dictionary();

    static const char* pickWord(unsigned length);
    static bool isWord(const char* string);
    static bool isOldWord(const char* word);

    static void sOnEvent(unsigned eventID, const EventData& data);

private:
    static char sOldWords[MAX_OLD_WORDS][MAX_LETTERS_PER_WORD + 1];
    static unsigned sNumOldWords;
    static unsigned sRandSeed;
};

#endif // DICTIONARY_H
