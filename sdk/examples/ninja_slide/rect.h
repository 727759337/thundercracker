/*
 * Rect
 *   isTouching
 *   intersection
 */   

#ifndef RECT_H
#define RECT_H

#include <sifteo.h>

using namespace Sifteo;

class Rect {

  public:

    Int2 origin;
    Int2 size;

    Rect(){
        origin = Vec2(0,0);
        size = Vec2(0,0);
    }

    Rect(int x, int y, int w, int h){
        origin.x = x;
        origin.y = y;
        size.x = w;
        size.y = h;
    }

    Rect(Int2 origin, Int2 size){
        this->origin = origin;
        this->size = size;
    }

    bool isEmpty(){
        return size.x <= 0 || size.y <= 0;
    }

    float right(){
        return origin.x + size.x - 1;
    }

    float bottom(){
        return origin.y + size.y - 1;
    }

    bool contains(Float2 &point){
        return !isEmpty()
            && origin.x <= point.x && point.x <= right()
            && origin.y <= point.y && point.y <= bottom();
    }

//     Rect intersection(Rect &other){
//         Rect result;

//         if (isEmpty() || other.isEmpty()){
//             result.origin = Vec2(0,0);
//             result.size = Vec2(0,0);
//             return result;
//         }

//         result.origin.x = MAX(origin.x, other.origin.x);
//         result.origin.y = MAX(origin.y, other.origin.y);
//         result.size.x = MIN(origin.x + size.x, other.origin.x + other.size.x) - result.origin.x;
//         result.size.y = MIN(origin.y + size.y, other.origin.y + other.size.y) - result.origin.y;

//         if (result.isEmpty()){
//             result.origin = Vec2(0,0);
//             result.size = Vec2(0,0);
//         }
//         return result;
//     }

    bool isTouching(Rect &other){
        return !isEmpty() && !other.isEmpty()
            && origin.x <= other.right() && right() >= other.origin.x
            && origin.y <= other.bottom() && bottom() >= other.origin.y;
    }


};

const Rect EMPTY_RECT(0,0,0,0);

#endif // RECT_H
