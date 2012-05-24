/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _FRONTEND_H
#define _FRONTEND_H

#include "gl_renderer.h"
#include "system.h"
#include "frontend_cube.h"
#include "frontend_mothership.h"
#include "frontend_overlay.h"
#include "tinythread.h"

#include <Box2D/Box2D.h>
#include <string>
#include <glfw.h>


class FrameRateController {
public:
    FrameRateController();
    void endFrame();

    void setTargetFPS(double target) {
        targetFPS = target;
    }

private:
    double lastTimestamp;
    double accumulator;
    double targetFPS;
};


class Frontend {
 public:
    Frontend();

    bool init(System *sys);

    static bool runFrame();
    static void exit();

    static void postMessage(std::string msg);

 private:
    /*
     * Number of real frames per virtual LCD frame (Assume 60Hz
     * real, 30 Hz virtual). This doesn't have any bearing on the
     * rendering frame rate of the cubes, it's just used for
     * generating vertical sync signals.
     */
    static const unsigned FRAME_HZ_DIVISOR = 2;

    class MousePicker : public b2QueryCallback {
    public:
        void test(b2World &world, b2Vec2 point);
        bool ReportFixture(b2Fixture *fixture);
        b2Vec2 mPoint;
        FrontendCube *mCube;
    };

    class ContactListener : public b2ContactListener {
    public:
        ContactListener(Frontend &fe) : frontend(fe) {};
        void BeginContact(b2Contact *contact);
        void EndContact(b2Contact *contact);
    private:
        Frontend &frontend;
        void updateSensors(b2Contact *contact, bool touching);
    };

    void animate();
    void draw();
    void updateCubeCount();

    bool openWindow(int width, int height, bool fullscreen=false);
    void toggleFullscreen();
    
    static void GLFWCALL onResize(int width, int height);
    static void GLFWCALL onKey(int key, int state);
    static void GLFWCALL onMouseMove(int x, int y);
    static void GLFWCALL onMouseButton(int button, int state);
    static void GLFWCALL onMouseWheel(int pos);
    static int GLFWCALL onWindowClose();

    void onMouseDown(int button);
    void onMouseUp(int button);
    void hoverOrRotate();

    void scaleViewExtent(float ratio);
    void createWalls();
    void moveWalls(bool immediate=false);
    void pushBodyTowards(b2Body *b, b2Vec2 target, float gain);

    static b2Vec2 getCubeGridLoc(unsigned index, unsigned total);
    b2Body *newKBox(float x, float y, float hw, float hh);
    unsigned cubeID(FrontendCube *cube);

    float zoomedViewExtent();
    float targetViewExtent();
    float pixelViewExtent();
    unsigned pixelZoomMode();

    b2Vec2 targetViewCenter();
    b2Vec2 mouseVec(float viewExtent);
    b2Vec2 worldToScreen(b2Vec2 world);
    float worldToScreen(float x);

    std::string createScreenshotName();
    void drawOverlay();

    void toggleRotationLock();

    System *sys;
    unsigned frameCount;
    unsigned idleFrames;

    // Object counts, local to the Frontend. (May lag the simulation thread)
    unsigned mothershipCount;
    unsigned cubeCount;

    FrontendMothership motherships[1];
    FrontendCube cubes[System::MAX_CUBES];

    bool toggleZoom;
    bool isFullscreen;
    bool isRunning;
    bool isRotationFixed;
    bool isAnimatingNewCubeLayout;

    int mouseX, mouseY;
    int mouseWheelPos;
    unsigned lastWindowW, lastWindowH;

    float viewExtent;
    float normalViewExtent;
    float maxViewExtent;
    b2Vec2 viewCenter;

    b2World world;
    b2Body *walls[4];

    b2Body *mouseBody;
    b2RevoluteJoint *mouseJoint;
    float spinTarget;
    bool mouseIsAligning;
    bool mouseIsSpinning;
    bool mouseIsPulling;
    bool mouseIsTilting;
    
    MousePicker mousePicker;
    ContactListener contactListener;
    FrameRateController frControl;

    GLRenderer renderer;
    FrontendOverlay overlay;

    static Frontend *instance;
    static tthread::mutex instanceLock;
};

#endif
