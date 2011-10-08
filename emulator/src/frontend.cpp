/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "frontend.h"


void Frontend::init(System *_sys)
{
    sys = _sys;
    frameCount = 0;
    toggleZoom = false;
    viewExtent = targetViewExtent() * 3.0;

    for (unsigned i = 0; i < sys->opt_numCubes; i++) {
        // Put all the cubes in a line
        float x =  ((sys->opt_numCubes - 1) * -0.5 + i) * FrontendCube::SIZE * 2.7;

        cubes[i].init(&sys->cubes[i], Point(x, 0));
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WM_SetCaption("Thundercracker", NULL);

}

void Frontend::exit()
{             
    SDL_Quit();
}

void Frontend::run()
{
    if (!onResize(800, 600))
        return;

    while (1) {
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
                onMouseUpdate(event.motion.x, event.motion.y, event.motion.state);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                onMouseUpdate(event.button.x, event.button.y, event.button.state);
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

    surface = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | SDL_RESIZABLE);
    if (surface == NULL) {
        fprintf(stderr, "Error creating SDL surface!\n");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);

    viewportWidth = width;
    viewportHeight = height;
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
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

void Frontend::onMouseUpdate(int x, int y, int buttons)
{
    int halfWidth = viewportWidth / 2;
    int halfHeight = viewportHeight / 2;
    float mouseScale = normalViewExtent() / (float)halfWidth;

    mouseVec = b2Vec2((x - halfWidth) * mouseScale,
                      (y - halfHeight) * mouseScale);
}

void Frontend::animate()
{
    const float easeSpeed = 0.1;

    viewExtent += easeSpeed * (targetViewExtent() - viewExtent);
    viewCenter += easeSpeed * (targetViewCenter() - viewCenter);
}

void Frontend::draw()
{
    // Background
    glClearColor(0.2, 0.2, 0.4, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Orthogonal camera
    glLoadIdentity();
    glScalef(1.0f / viewExtent,
             -viewportWidth / (float)viewportHeight / viewExtent,
             1.0f / viewExtent);
    glTranslatef(-viewCenter.x, -viewCenter.y, 0);

    // All cubes
    for (unsigned i = 0; i < sys->opt_numCubes; i++)
        cubes[i].draw();

    SDL_GL_SwapBuffers();
}
