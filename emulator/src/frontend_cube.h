/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _FRONTEND_CUBE_H
#define _FRONTEND_CUBE_H

#include "system.h"
#include <Box2D/Box2D.h>

class FrontendCube;


class AccelerationProbe {
 public:
    AccelerationProbe();

    b2Vec3 measure(b2Body *body, float unitsToGs);

 private:
    static const unsigned filterWidth = 8;
    unsigned head;
    b2Vec2 velocitySamples[filterWidth];
};


struct FixtureData {
    enum Type {
        T_CUBE = 0,
        T_NEIGHBOR,
    };

    Type type;
    FrontendCube *cube;
    Cube::Neighbors::Side side;
};


class FrontendCube {
 public:
    FrontendCube();

    void init(unsigned id, Cube::Hardware *hw, b2World &world, float x, float y);
    void exit();

    void animate();
    bool draw(GLRenderer &r, bool pixelAccurate, float pixelSize);

    void setTiltTarget(b2Vec2 angles);
    void updateNeighbor(bool touching, unsigned mySide,
                        unsigned otherSide, unsigned otherCube);

    void setHoverTarget(float h);
    
    void setTouch(float amount) {
        hw->setTouch(amount);
    }
    
    bool isHovering() {
        return hoverTarget > HEIGHT;
    }
    
    unsigned getId() const {
        return id;
    }
    
    bool isInitialized() const {
        return body != 0;
    }

    void computeAABB(b2AABB &aabb);
    
    b2Body *body;

    /*
     * Size of the cube, in Box2D "meters". Our rendering parameters
     * are scaled relative to this size, so its affect is really
     * mostly about the units we use for the physics sim.
     */
    static const float SIZE = 0.5;

    /*
     * Height of the cube, relative to its half-width
     */
    static const float HEIGHT = 0.6;

    /*
     * Size of the portion of the cube we're calling the "center". Has
     * some UI implications, like dragging on the center vs. a corner.
     * This is the radius of a circle, at the cube's center of mass.
     *
     * This is in cube-sized units (will be multiplied with SIZE to get world coordinates).
     */
    static const float CENTER_SIZE = 0.6;
    
    /*
     * Size of the portion of the cube covered by the LCD image.
     *
     * This is in cube-sized units (will be multiplied with SIZE to get world coordinates).
     */
    static const float LCD_SIZE = 0.645;

    /*
     * The sensitive region for this cube's neighbor transceivers.
     *
     * Both relative to SIZE. These regions are modeled as circles,
     * and whenever two circles *touch* the sensor is active. This is
     * in contrast to the usual definition of sensor range, in which
     * the center point of one sensor must be within the range of the
     * other sensor. To compensate, this radius should be 1/2 the
     * sensor's actual range.
     */
    static const float NEIGHBOR_CENTER = 0.9;
    static const float NEIGHBOR_RADIUS = 0.15;
    
    /*
     * We use different hover heights to signify various UI actions.
     * A very slight hover means the cube is being manipulated. A more
     * extreme hover means it's actually being picked up high enough
     * to not touch non-hovering cubes.
     */
    static const float HOVER_NONE = 0.0;
    static const float HOVER_SLIGHT = 0.3;
    static const float HOVER_FULL = 2.0;

 private:
    void initBody(b2World &world, float x, float y);
    void initNeighbor(Cube::Neighbors::Side side, float x, float y);

    b2Fixture *bodyFixture;
    FixtureData bodyFixtureData;
    FixtureData neighborFixtureData[Cube::Neighbors::NUM_SIDES];

    AccelerationProbe accel;
    b2Vec2 tiltTarget;
    b2Vec2 tiltVector;
    float hover, hoverTarget;
    b2Mat33 modelMatrix;

    unsigned id;
    Cube::Hardware *hw;
    
    uint32_t lastLcdCookie;
};

#endif
