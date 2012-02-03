#include "PrototypeWordList.h"
#include "PrototypeWordListData.h"
#include <sifteo.h>
#include "WordGame.h"


/*
 * XXX: Only used for bsearch() currently. We should think about what kind of low-level VM
 *      primitives the search should be based on (with regard to ABI, as well as cache
 *      behavior) and design it with a proper syscall interface. But for now, we're leaking
 *      some libc code into the game :(
 */
#include <stdlib.h>

//#define DAWG_TEST 1
#ifdef DAWG_TEST
//static const int sDAWG[22000] = {};

static const char protoWordList[][7] =
{
"aa",
"aah",
};

#else

// TODO a spell check representation that expands to higher number of letters
// while allowing for breaking into pieces to be loaded from flash
// (possibly pieces of a DAWG dictionary representation, up to 15 letters or so)
//
// 5 bits per letter, up to 6 letter words, plus flags
extern const uint32_t protoWordList[];
extern const unsigned PROTO_WORD_LIST_LENGTH;

#endif

PrototypeWordList::PrototypeWordList()
{
}


bool PrototypeWordList::pickWord(char* buffer)
{
    unsigned startIndex = WordGame::random.randrange(PROTO_WORD_LIST_LENGTH);
    unsigned i = startIndex;
    do
    {
        // if the 3 seed word bits are set to this num cubes
        if (((protoWordList[i] >> 31)))
        {
            char word[MAX_LETTERS_PER_WORD + 1];
            if (!bitsToString(protoWordList[i], word))
            {
                ASSERT(0);
                return false;
            }

            if (_SYS_strnlen(word, MAX_LETTERS_PER_WORD + 1) ==
                    GameStateMachine::getCurrentMaxLettersPerWord())
            {
                 _SYS_strlcpy(buffer, word, GameStateMachine::getCurrentMaxLettersPerWord() + 1);
                return true;
            }
        }
        i = (i + 1) % PROTO_WORD_LIST_LENGTH;
    } while (i != startIndex);

    ASSERT(0);
    return false;
}

static int bsearch_strcmp(const void*a, const void*b)
{
    // a is pKey in previous call, char**
    //STATIC_ASSERT(sizeof(uint32_t) == sizeof(unsigned));
    uint32_t* pb = (uint32_t*)b;
#if DEBUG
    const char** pa = (const char**)a;
    //printf("a %s, b %s\n", *(const char **)a, (const char *)b);
#endif

    // FIXME, if using a bsearch with compressed string in the future, don't
    // reconstruct strings, just make the hash and compare the smaller rep (assuming cheaper)
    char word[MAX_LETTERS_PER_WORD + 1];
    if (PrototypeWordList::bitsToString(*pb, word))
    {
        return _SYS_strncmp(*(const char **)a, word, GameStateMachine::getCurrentMaxLettersPerWord() + 1);
    }
    ASSERT(0);
    return 0;
}

bool PrototypeWordList::isWord(const char* string, bool& isBonus)
{
#ifndef DAWG_TEST
    //STATIC_ASSERT(arraysize(protoWordList) == 28839);
#endif
    const char** pKey = &string;
    const uint32_t* array = (uint32_t*)protoWordList;
    //const char** pArray = &array;
    const uint32_t* pItem =
            (const uint32_t*) bsearch(
                pKey,
                array,
                PROTO_WORD_LIST_LENGTH,
                sizeof(protoWordList[0]),
                (int(*)(const void*,const void*)) bsearch_strcmp);

    if (pItem != NULL)
    {
        isBonus = !((*pItem) & (1 << 31));
        return true;
    }
    return false;
}

bool PrototypeWordList::bitsToString(uint32_t bits, char* buffer)
{
    char word[MAX_LETTERS_PER_WORD + 1];
    _SYS_memset8((uint8_t*)word, 0, sizeof(word));
    const unsigned LTR_MASK = 0x1f; // 5 bits per letter
    const unsigned BITS_PER_LETTER = 5;
    // TODO store dictionary differently to allow for longer than 6 letter words
    for (unsigned j = 0; j < MAX_LETTERS_PER_WORD && j < 32/BITS_PER_LETTER; ++j)
    {
        char letter = 'A' - 1 + ((bits >> (j * BITS_PER_LETTER)) & LTR_MASK);
        if (letter < 'A' || letter > 'Z')
        {
            break;
        }
        word[j] = letter;
    }
    _SYS_strlcpy(buffer, word, arraysize(word));
    return buffer[0] != '\0';
}
