/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the public interface to the Sifteo SDK.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#ifndef _SIFTEO_MATH_H
#define _SIFTEO_MATH_H

#include <sifteo/abi.h>
#include <math.h>

namespace Sifteo {
namespace Math {

/**
 * 2-element float vector
 */

struct Float2 {
    Float2()
        : x(0), y(0) {}

    Float2(float _x, float _y)
        : x(_x), y(_y) {}

    static Float2 polar(float angle, float magnitude) {
        Float2 f;
        f.setPolar(angle, magnitude);
        return f;
    }

    void set(float _x, float _y) {
        x = _x;
        y = _y;
    }

    void setPolar(float angle, float magnitude) {
        x = cosf(angle) * magnitude;
        y = sinf(angle) * magnitude;
    }

    inline float len2() { return ( x * x + y * y ); }
    
    float x, y;
};

inline Float2 operator+(const Float2& u, const Float2& v) { return Float2(u.x+v.x, u.y+v.y); }
inline Float2 operator += (Float2& u, const Float2& v) { return Float2(u.x+=v.x, u.y+=v.y); }
inline Float2 operator-(const Float2& u, const Float2& v) { return Float2(u.x-v.x, u.y-v.y); }
inline Float2 operator -= (Float2& u, const Float2& v) { return Float2(u.x-=v.x, u.y-=v.y); }
inline Float2 operator*(float k, const Float2& v) { return Float2(k*v.x, k*v.y); }
inline Float2 operator*(const Float2& v, float k) { return Float2(k*v.x, k*v.y); }
inline Float2 operator*(const Float2& u, const Float2& v) { return Float2(u.x*v.x-u.y*v.y, u.y*v.x+u.x*v.y); } // complex multiplication
inline Float2 operator/(const Float2& u, float k) { return Float2(u.x/k, u.y/k); }
inline Float2 operator += (Float2& u, float k) { return Float2(u.x+=k, u.y+=k); }
inline Float2 operator *= (Float2& u, float k) { return Float2(u.x*=k, u.y*=k); }
inline bool operator==(const Float2& u, const Float2& v) { return u.x == v.x && u.y == v.y; }
inline bool operator!=(const Float2& u, const Float2& v) { return u.x != v.x || u.y != v.y; }

/**
 * 2-element integer vector
 */

struct Vec2 {
	Vec2()
        : x(0), y(0) {}

	Vec2(int _x, int _y)
        : x(_x), y(_y) {}

    void set(int _x, int _y) {
        x = _x;
        y = _y;
    }

	// Implicit conversion from float (truncation)
	Vec2(const Float2 &other)
        : x((int)other.x), y((int)other.y) {}

    // Explicit rounding
    static Vec2 round(const Float2 &other) {
        return Vec2((int)(other.x + 0.5f), (int)(other.y + 0.5f));
    }

    int x, y;
};

inline Vec2 operator+(const Vec2& u, const Vec2& v) { return Vec2(u.x+v.x, u.y+v.y); }
inline Vec2 operator += (Vec2& u, const Vec2& v) { return Vec2(u.x+=v.x, u.y+=v.y); }
inline Vec2 operator-(const Vec2& u, const Vec2& v) { return Vec2(u.x-v.x, u.y-v.y); }
inline Vec2 operator -= (Vec2& u, const Vec2& v) { return Vec2(u.x-=v.x, u.y-=v.y); }
inline Vec2 operator*(int k, const Vec2& v) { return Vec2(k*v.x, k*v.y); }
inline Vec2 operator*(const Vec2& v, int k) { return Vec2(k*v.x, k*v.y); }
inline Vec2 operator*(const Vec2& u, const Vec2& v) { return Vec2(u.x*v.x-u.y*v.y, u.y*v.x+u.x*v.y); } // complex multiplication
inline Vec2 operator/(const Vec2& u, const int k) { return Vec2(u.x/k, u.y/k); }
inline bool operator==(const Vec2& u, const Vec2& v) { return u.x == v.x && u.y == v.y; }
inline bool operator!=(const Vec2& u, const Vec2& v) { return u.x != v.x || u.y != v.y; }



/**
 * Integer rectangle, stored as Left Top Right Bottom (LTRB).
 */

struct Rect {
    Rect(int _left, int _top, int _right, int _bottom)
        : left(_left), top(_top), right(_right), bottom(_bottom) {}

    int left, top, right, bottom;
};


/**
 * Integer rectangle, stored as an origin point and width/height.
 */

struct RectWH {
    RectWH(int _x, int _y, int _width, int _height)
        : x(_x), y(_y), width(_width), height(_height) {}

    int x, y, width, height;
};


/**
 * Pseudo-random number generator.
 * Each instance of the Random class has a distinct PRNG state.
 *
 * When possible, method semantics here have been designed to match
 * those used by Python's "random" module.
 */
 
struct Random {
    _SYSPseudoRandomState state;

    /**
     * Construct a new random number generator, using an arbitrary seed.
     */

    Random() {
        seed();
    }
    
    /**
     * Construct a new random number generator with a well-defined seed.
     */
     
    Random(uint32_t s) {
        seed(s);
    }
    
    /**
     * Re-seed this random number generator. For a given seed, the subsequent
     * random numbers are guaranteed to be deterministic.
     */
    
    void seed(uint32_t s) {
        _SYS_prng_init(&state, s);
    }

    /**
     * Re-seed this random number generator arbitrarily.
     * This implementation uses the system's nanosecond timer.
     */
    
    void seed() {
        int64_t nanosec;
        _SYS_ticks_ns(&nanosec);
        seed((uint32_t) nanosec);
    }

    /**
     * Returns the next raw 32-bit pseudo-random number
     */
    
    uint32_t raw() {
        return _SYS_prng_value(&state);
    }
    
    /**
     * Returns a uniformly distributed floating point number between 0 and 1, inclusive.
     */
    
    float random() {
        return raw() * (1.0f / 0xFFFFFFFF);
    }

    /**
     * Returns a uniformly distributed floating point number in the range [a, b) or
     * [a, b], depending on rounding.
     */
    
    float uniform(float a, float b) {
        // Order of operations here allows constant folding if the endpoints are constant
        return a + raw() * ((b-a) / 0xFFFFFFFF);
    }

    /**
     * Returns a uniformly distributed random integer in the range [a, b], including both
     * end points.
     */
    
    int randint(int a, int b) {
        return a + _SYS_prng_valueBounded(&state, b - a);
    }

    unsigned randint(unsigned a, unsigned b) {
        return a + _SYS_prng_valueBounded(&state, b - a);
    }

    /**
     * Returns a uniformly distributed random integer in the half-open interval [a, b),
     * including the lower but not the upper end point.
     */
    
    int randrange(int a, int b) {
        return randint(a, b - 1);
    }

    unsigned randrange(unsigned a, unsigned b) {
        return randint(a, b - 1);
    }
    
    /**
     * The one-argument variant of randrange() always starts at zero, and
     * returns an integer up to but not including 'count'. It is guaranteed
     * to be capable of returning 'count' distinct values, starting at zero.
     */

    int randrange(int count) {
        return randrange(0, count);
    }

    unsigned randrange(unsigned count) {
        return randrange((unsigned)0, count);
    }

};


/**
 * An augmented 3x2 matrix, for doing 2D affine transforms.
 *
 *  [ xx  yx  cx ]
 *  [ xy  yy  cy ]
 *  [  0   0   1 ]
 *
 * The way we use affine transforms for background scaling are
 * very similiar to the mechanism used by the GameBoy Advance
 * PPU. There's a great tutorial on this at:
 *
 * http://www.coranac.com/tonc/text/affine.htm
 */

struct AffineMatrix {
    float cx, cy;
    float xx, xy;
    float yx, yy;

    AffineMatrix() {}

    AffineMatrix(float _xx, float _yx, float _cx,
                 float _xy, float _yy, float _cy)
        : cx(_cx), cy(_cy), xx(_xx),
          xy(_xy), yx(_yx), yy(_yy) {}
    
    static AffineMatrix identity() {
        return AffineMatrix(1, 0, 0,
                            0, 1, 0);
    }

    static AffineMatrix scaling(float s) {
        float inv_s = 1.0f / s;
        return AffineMatrix(inv_s, 0, 0,
                            0, inv_s, 0);
    }

    static AffineMatrix translation(float x, float y) {
        return AffineMatrix(1, 0, x,
                            0, 1, y);
    }

    static AffineMatrix rotation(float angle) {
        float s = sinf(angle);
        float c = cosf(angle);
        return AffineMatrix(c, -s, 0,
                            s, c, 0);
    }

    void operator*= (const AffineMatrix &m) {
        AffineMatrix n;

        n.cx = xx*m.cx + yx*m.cy + cx;
        n.cy = xy*m.cx + yy*m.cy + cy;
        n.xx = xx*m.xx + yx*m.xy;
        n.xy = xy*m.xx + yy*m.xy;
        n.yx = xx*m.yx + yx*m.yy;
        n.yy = xy*m.yx + yy*m.yy;

        *this = n;
    }

    void translate(float x, float y) {
        *this *= translation(x, y);
    }

    void rotate(float angle) {
        *this *= rotation(angle);
    }
    
    void scale(float s) {
        *this *= scaling(s);
    }
};


/*
 * General helper routines
 */

template <typename T> inline T clamp(const T& value, const T& low, const T& high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

template <typename T> inline T abs(const T& value)
{
    if (value < 0) {
        return -value;
    }
    return value;
}


}   // namespace Math
}   // namespace Sifteo

#endif
