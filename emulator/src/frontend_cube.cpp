/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "frontend.h"

const float FrontendCube::SIZE;


void FrontendCube::init(unsigned _id, Cube::Hardware *_hw, b2World &world, float x, float y)
{
    id = _id;
    hw = _hw;

    tiltTarget.Set(0,0);
    tiltVector.Set(0,0);

    initBody(world, x, y);

    initNeighbor(Cube::Neighbors::TOP, 0, -1);
    initNeighbor(Cube::Neighbors::BOTTOM, 0, 1);

    initNeighbor(Cube::Neighbors::LEFT, -1, 0);
    initNeighbor(Cube::Neighbors::RIGHT, 1, 0);
}

void FrontendCube::initBody(b2World &world, float x, float y)
{
    /*
     * Pick our physical properties carefully: We want the cubes to
     * stop when you let go of them, but a little bit of realistic
     * physical jostling is nice.  Most importantly, we need a
     * corner-drag to actually rotate the cubes effortlessly. This
     * means relatively high linear damping, and perhaps lower angular
     * damping.
     */ 

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.linearDamping = 30.0f;
    bodyDef.angularDamping = 5.0f;
    bodyDef.position.Set(x, y);
    body = world.CreateBody(&bodyDef);

    b2PolygonShape box;
    const float boxSize = SIZE * 0.96;    // Compensate for polygon 'skin'
    box.SetAsBox(boxSize, boxSize);
    
    bodyFixtureData.type = FixtureData::T_CUBE;
    bodyFixtureData.cube = this;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.8f;
    fixtureDef.userData = &bodyFixtureData;
    body->CreateFixture(&fixtureDef);
}

void FrontendCube::initNeighbor(Cube::Neighbors::Side side, float x, float y)
{
    /*
     * Neighbor sensors are implemented using Box2D sensors. These are
     * four additional fixtures attached to the same body.
     */

    b2CircleShape circle;
    circle.m_p.Set(x * NEIGHBOR_CENTER * SIZE, y * NEIGHBOR_CENTER * SIZE);
    circle.m_radius = NEIGHBOR_RADIUS * SIZE;

    neighborFixtureData[side].type = FixtureData::T_NEIGHBOR;
    neighborFixtureData[side].cube = this;
    neighborFixtureData[side].side = side;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.isSensor = true;
    fixtureDef.userData = &neighborFixtureData[side];
    body->CreateFixture(&fixtureDef);
}

void FrontendCube::draw(GLRenderer &r)
{
    r.drawCube(id, body->GetPosition(), body->GetAngle(), tiltVector,
               hw->lcd.isVisible() ? hw->lcd.fb_mem : NULL,
               modelMatrix);
}

void FrontendCube::setTiltTarget(b2Vec2 angles)
{
    /* Target tilt angles that we're animating toward, as YX Euler angles in degrees */
    tiltTarget = angles;
}

void FrontendCube::updateNeighbor(bool touching, unsigned mySide,
                                  unsigned otherSide, unsigned otherCube)
{
    if (touching)
        hw->neighbors.setContact(mySide, otherSide, otherCube);
    else
        hw->neighbors.clearContact(mySide, otherSide, otherCube);
}

void FrontendCube::animate()
{
    /* Animated tilt */
    const float tiltGain = 0.25f;
    tiltVector += tiltGain * (tiltTarget - tiltVector);

    /*
     * Make a 3-dimensional acceleration vector which also accounts
     * for gravity. We're now measuring it in G's, so we apply an
     * arbitrary conversion from our simulated units (Box2D "meters"
     * per timestep) to something realistic.
     *
     * In our coordinate system, +Z is toward the camera, and -Z is
     * down, into the table.
     */

    b2Vec3 accelG = accel.measure(body, 0.06f);

    /*
     * Now use the same matrix we computed while rendering the last frame, and
     * convert this acceleration into device-local coordinates. This will take into
     * account tilting, as well as the cube's angular orientation.
     */

    b2Vec3 accelLocal = modelMatrix.Solve33(accelG);
    hw->setAcceleration(accelLocal.x, accelLocal.y);
}

AccelerationProbe::AccelerationProbe()
{
    for (unsigned i = 0; i < filterWidth; i++)
        velocitySamples[i].Set(0,0);
    head = 0;
}

b2Vec3 AccelerationProbe::measure(b2Body *body, float unitsToGs)
{
    b2Vec2 prevV = velocitySamples[head];
    b2Vec2 currentV = body->GetLinearVelocity();
    velocitySamples[head] = currentV;
    head = (head + 1) % filterWidth;

    // Take a finite derivative over the width of our filter window
    b2Vec2 accel2D = currentV - prevV;

    return b2Vec3(accel2D.x * unitsToGs, accel2D.y * unitsToGs, -1.0f);
}
