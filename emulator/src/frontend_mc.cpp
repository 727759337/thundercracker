/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * Max Kaufmann <max@sifteo.com>
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "frontend_mc.h"
#include "gl_renderer.h"

FrontendMC::FrontendMC()
    : body(0) {}

void FrontendMC::init(b2World& world, float x, float y)
{
    /*
     * Note: The MC intentionally has very different physical
     *       properties than the cubes. We don't want the MC
     *       to be super-spinny like the cubes, so we apply
     *       a more typical amount of angular damping, and
     *       it needs to feel a bit "heavier" overall.
     */
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.linearDamping = 80.0f;
    bodyDef.angularDamping = 1000.0f;
    bodyDef.position.Set(x, y);
    body = world.CreateBody(&bodyDef);

    b2PolygonShape box;
    const b2Vec2 boxSize = 0.96f * b2Vec2(
        MCConstants::SIZEX,
        MCConstants::SIZEY
    );
    box.SetAsBox(boxSize.x, boxSize.y);

    bodyFixtureData.type = FixtureData::T_MC;
    bodyFixtureData.ptr.mc = this;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.8f;
    fixtureDef.userData = &bodyFixtureData;
    bodyFixture = body->CreateFixture(&fixtureDef);
}

void FrontendMC::exit()
{
    if (body) {
        body->GetWorld()->DestroyBody(body);
        body = 0;
    }
}

void FrontendMC::draw(GLRenderer &r)
{
    static const float dummyled[3] = { 1.0, 1.0, 1.0 };

    r.drawMC(body->GetPosition(), body->GetAngle(), dummyled);
}

void FrontendMC::setButtonPressed(bool isDown)
{
    // TODO
}