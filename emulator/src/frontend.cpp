/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "frontend.h"


class QueryCallback : public b2QueryCallback {
 public:
    QueryCallback(const b2Vec2& point)
        : mPoint(point), mFixture(NULL) {}

    bool ReportFixture(b2Fixture *fixture) {
        b2Body *body = fixture->GetBody();

        if (body->GetType() == b2_dynamicBody &&
            fixture->TestPoint(mPoint)) {
            mFixture = fixture;
            return false;
        }
        return true;
    }
            
    b2Vec2 mPoint;
    b2Fixture *mFixture;
};


Frontend::Frontend() 
  : world(b2Vec2(0.0f, 0.0f)),
    mouseBody(NULL),
    mouseJoint(NULL),
    mouseIsAligning(false)
{}


void Frontend::init(System *_sys)
{
    sys = _sys;
    frameCount = 0;
    toggleZoom = false;
    viewExtent = targetViewExtent() * 3.0;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WM_SetCaption("Thundercracker", NULL);

    /*
     * Create cubes
     */

    for (unsigned i = 0; i < sys->opt_numCubes; i++) {
        // Put all the cubes in a line
        float x =  ((sys->opt_numCubes - 1) * -0.5 + i) * FrontendCube::SIZE * 2.7;

        cubes[i].init(&sys->cubes[i], world, x, 0);
    }

    /*
     * Create the rest of the world
     */

    float extent = normalViewExtent();
    float extent2 = extent * 2.0f;

    // Thick walls
    newStaticBox(extent2, 0, extent, extent2);   // X+
    newStaticBox(-extent2, 0, extent, extent2);  // X-
    newStaticBox(0, extent2, extent2, extent);   // Y+
    newStaticBox(0, -extent2, extent2, extent);  // Y-
}

void Frontend::newStaticBox(float x, float y, float hw, float hh)
{
    b2BodyDef bodyDef;
    bodyDef.position.Set(x, y);
    b2Body *body = world.CreateBody(&bodyDef);

    b2PolygonShape box;
    box.SetAsBox(hw, hh);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    body->CreateFixture(&fixtureDef);
}

void Frontend::exit()
{             
    SDL_Quit();
}

void Frontend::run()
{
    if (sys->opt_numCubes > 1) {
        // 2 or more cubes: Large window
        if (!onResize(800, 600))
            return;
    } else {    
        // Zero or one cube: Small window
        if (!onResize(300, 300))
            return;
    }

    while (sys->isRunning()) {
        SDL_Event event;
    
        // Drain the GUI event queue
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                
            case SDL_QUIT:
                return;

            case SDL_VIDEORESIZE:
                if (!onResize(event.resize.w, event.resize.h))
                    return;
                break;

            case SDL_KEYDOWN:
                onKeyDown(event.key);
                break;

            case SDL_MOUSEMOTION:
                mouseX = event.motion.x;
                mouseY = event.motion.y;
                break;

            case SDL_MOUSEBUTTONDOWN:
                mouseX = event.button.x;
                mouseY = event.button.y;
                onMouseDown(event.button.button);
                break;

            case SDL_MOUSEBUTTONUP:
                mouseX = event.button.x;
                mouseY = event.button.y;
                onMouseUp(event.button.button);
                break;

            }
        }

        if (sys->time.isPaused())
            SDL_Delay(50);
        
        if (!(frameCount % FRAME_HZ_DIVISOR)) {
            for (unsigned i = 0; i < sys->opt_numCubes; i++)
                sys->cubes[i].lcd.pulseTE();
        }

        animate();
        draw();
        frameCount++;
    }
}

bool Frontend::onResize(int width, int height)
{
    if (!(width && height))
        return true;

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); 
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);

    surface = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | SDL_RESIZABLE);

    if (surface == NULL) {
        // First failure? Try without FSAA.

        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0); 
        surface = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | SDL_RESIZABLE);
        if (surface) {
            fprintf(stderr, "Warning: FSAA not available. Your cubes will have crunchy edges :(\n");
        }
    }

    if (surface == NULL) {
        fprintf(stderr, "Error creating SDL surface!\n");
        return false;
    }

    viewportWidth = width;
    viewportHeight = height;
    glViewport(0, 0, width, height);
    
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    for (unsigned i = 0; i < sys->opt_numCubes; i++)
        cubes[i].initGL();

    return true;
}

void Frontend::onKeyDown(SDL_KeyboardEvent &evt)
{
    switch (evt.keysym.sym) {
        
    case 'z':
        toggleZoom = !toggleZoom;
        break;

    default:
        break;
    }
}

void Frontend::onMouseDown(int button)
{
    if (!mouseBody) {
        // Test a small bounding box at the mouse hotspot

        b2AABB aabb;
        b2Vec2 mouse = mouseVec(normalViewExtent());
        b2Vec2 size(0.001f, 0.001f);
        aabb.lowerBound = mouse - size;
        aabb.upperBound = mouse + size;

        QueryCallback callback(mouse);
        world.QueryAABB(&callback, aabb);

        if (callback.mFixture) {
            // This is the body underneath the mouse
            mousePickedBody = callback.mFixture->GetBody();
            
            // This body represents the mouse itself now as a physical object
            b2BodyDef mouseDef;
            mouseDef.type = b2_kinematicBody;
            mouseDef.position = mouse;
            mouseDef.allowSleep = false;
            mouseBody = world.CreateBody(&mouseDef);

            if ((button & 2) || (SDL_GetModState() & KMOD_SHIFT)) {
                /*
                 * If this was a right-click or shift-click, go into tilting mode
                 */
                
                mouseIsTilting = true;

            } else {
                /*
                 * Otherwise, the mouse is pulling.
                 *
                 * We represent our mouse as a kinematic body. This
                 * lets us pull/push a cube, or by grabbing it at an
                 * edge or corner, rotate it intuitively.
                 */
                
                mouseIsPulling = true;

                /*
                 * Pick an attachment point. If we're close to the center,
                 * snap our attachment point to the center of mass, and
                 * turn on a servo that tries to orient the cube to a
                 * multiple of 90 degrees.
                 */
                
                b2Vec2 anchor = mouse;
                b2Vec2 center = mousePickedBody->GetWorldCenter();
                if (b2Distance(anchor, center) < FrontendCube::SIZE * FrontendCube::CENTER_SIZE) {
                    anchor = center;
                    mouseIsAligning = true;
                }

                // Glue it to the point we picked, with a revolute joint

                b2RevoluteJointDef jointDef;
                jointDef.Initialize(mousePickedBody, mouseBody, anchor);
                jointDef.motorSpeed = 0.0f;
                jointDef.maxMotorTorque = 10.0f;
                jointDef.enableMotor = true;
                mouseJoint = (b2RevoluteJoint*) world.CreateJoint(&jointDef);
            }
        }
    }
}

void Frontend::onMouseUp(int button)
{
    if (mouseBody) {
        FrontendCube *cube = FrontendCube::fromBody(mousePickedBody);
        if (cube) {
            // Reset tilt
            cube->setTiltTarget(b2Vec2(0.0f, 0.0f));
        }

        /* Mouse state reset */

        if (mouseJoint)
            world.DestroyJoint(mouseJoint);
        world.DestroyBody(mouseBody);
        mouseJoint = NULL;
        mouseBody = NULL;
        mousePickedBody = NULL;
        mouseIsAligning = false;
        mouseIsPulling = false;
        mouseIsTilting = false;
    }
}

void Frontend::animate()
{
    /*
     * All of our animation is fixed timestep.
     */

    const float timeStep = 1.0f / 60.0f;
    const int velocityIterations = 10;
    const int positionIterations = 8;

    if (mouseIsPulling) {
        /*
         * We've attached a mouse body to the object we're
         * dragging. Move the mouse body to the current mouse
         * position. We don't want to just set the mouse position,
         * since that will cause any resulting collisions to simulate
         * incorrectly.
         *
         * The mouse velocity needs to be relatively high so that we
         * can shake the cubes vigorously, but too high and it'll
         * oscillate.
         */

        const float gain = 50.0f;

        b2Vec2 mouse = mouseVec(normalViewExtent());
        mouseBody->SetLinearVelocity(gain * (mouse - mouseBody->GetWorldCenter()));
    }

    if (mouseIsAligning) {
        /*
         * Turn on a joint motor, to try and push the cube toward its
         * closest multiple of 90 degrees.
         */
        
        const float gain = 20.0f;

        float cubeAngle = mouseJoint->GetBodyA()->GetAngle();
        float fractional = fmod(cubeAngle + M_PI/4, M_PI/2);
        if (fractional < 0) fractional += M_PI/2;
        float error = fractional - M_PI/4;
        mouseJoint->SetMotorSpeed(gain * error);
    }

    if (mouseIsTilting) {
        /*
         * Animate the tilt, using a pair of Euler angles based on the distance
         * between the mouse's current position and its initial grab point.
         */

        FrontendCube *cube = FrontendCube::fromBody(mousePickedBody);
        if (cube) {
            const float maxTilt = 80.0f;
            b2Vec2 mouseDiff = mouseVec(normalViewExtent()) - mouseBody->GetWorldCenter();
            b2Vec2 tiltTarget = (maxTilt / FrontendCube::SIZE) * mouseDiff; 
            tiltTarget.x = b2Clamp(tiltTarget.x, -maxTilt, maxTilt);
            tiltTarget.y = b2Clamp(tiltTarget.y, -maxTilt, maxTilt);        

            // Rotate it into the cube's coordinates
            cube->setTiltTarget(b2Mul(b2Rot(-mousePickedBody->GetAngle()),
                                      tiltTarget));
        }
    }
        
    /* Local per-cube animations */
    for (unsigned i = 0; i < sys->opt_numCubes; i++)
        cubes[i].animate();

    /* Animated viewport centering/zooming */
    {
        const float gain = 0.1;

        viewExtent += gain * (targetViewExtent() - viewExtent);
        viewCenter += gain * (targetViewCenter() - viewCenter);
    }

    world.Step(timeStep, velocityIterations, positionIterations);
}

void Frontend::draw()
{
    /*
     * Camera.
     *
     * Our simulation (and our mouse input!) is really 2D, but we use
     * perspective in order to make the whole scene feel a bit more
     * real, and especially to make tilting believable.
     *
     * We scale this so that the X axis is from -viewExtent to
     * +viewExtent at the level of the cube face.
     */

    float aspect = viewportHeight / (float)viewportWidth;
    float yExtent = aspect * viewExtent;

    float zPlane = FrontendCube::SIZE * FrontendCube::HEIGHT;
    float zCamera = 5.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float zDepth = zFar - zNear;
    
    GLfloat proj[16] = {
        zCamera / viewExtent,
        0,
        0,
        0,

        0,
        -zCamera / yExtent,
        0,
        0,

        0,
        0,
        -(zFar + zNear) / zDepth,
        -1.0f,

        0,
        0,
        -2.0f * (zFar * zNear) / zDepth,
        0,
    };

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);
    glTranslatef(-viewCenter.x, -viewCenter.y, -zCamera-zPlane);
    glMatrixMode(GL_MODELVIEW);

    /*
     * Background
     */

    glClearColor(0.2, 0.2, 0.4, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*
     * Cubes
     */

    for (unsigned i = 0; i < sys->opt_numCubes; i++)
        cubes[i].draw();

    SDL_GL_SwapBuffers();
}

float Frontend::zoomedViewExtent() {
    if (sys->opt_numCubes > 2) {
        // Zoom in one one cube
        return FrontendCube::SIZE * 1.1;
    } else {
        // High zoom on our one and only cube
        return FrontendCube::SIZE * 0.2;
    }
}

float Frontend::normalViewExtent() {
    return FrontendCube::SIZE * (sys->opt_numCubes * 1.5);
}

float Frontend::targetViewExtent() {
    return toggleZoom ? zoomedViewExtent() : normalViewExtent();
}    

b2Vec2 Frontend::targetViewCenter() {
    // When zooming in/out, the pixel under the cursor stays the same.
    return toggleZoom ? (mouseVec(normalViewExtent()) - mouseVec(targetViewExtent()))
        : b2Vec2(0.0, 0.0);
}

b2Vec2 Frontend::mouseVec(float viewExtent) {
    int halfWidth = viewportWidth / 2;
    int halfHeight = viewportHeight / 2;
    float mouseScale = viewExtent / (float)halfWidth;
    return  b2Vec2((mouseX - halfWidth) * mouseScale,
                   (mouseY - halfHeight) * mouseScale);
}
