#ifndef _TFUTILS_H
#define _TFUTILS_H

#define PRINTS_ON 0
#if PRINTS_ON
#define PRINT(...) printf (__VA_ARGS__)
#else
#define PRINT(...) 
#endif

namespace SelectorMenu
{

typedef enum
{
	UP,
	LEFT, 
	DOWN,
    RIGHT,
    NONE
} Side;


namespace Util
{

inline float Clamp( float value, float min, float max )
{
    if( value < min )
        return min;
    else if( value > max )
        return max;

    return value;
}

}

} //namespace SelectorMenu
#endif
