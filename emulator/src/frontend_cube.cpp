/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "frontend.h"

/*
 * Shader for rendering the LCD screens. We could probably do something
 * more complicated with simulating the subpixels in the LCD screens, but
 * right now I just have two goals:
 *
 *   1. See each pixel distinctly at >100% zooms
 *   2. No visible aliasing artifacts
 *
 * This uses a simple algorithm that decomposes texture coordinates
 * into a fractional and whole pixel portion, and applies a smooth
 * nonlinear interpolation across pixel boundaries:
 *
 * http://www.iquilezles.org/www/articles/texture/texture.htm
 */

const GLchar *FrontendCube::srcLcdVP[] = {
    "varying vec2 imageCoord;"
    "void main() {"
        "gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
        "imageCoord = gl_MultiTexCoord0.xy;"
    "}"
};

const GLchar *FrontendCube::srcLcdFP[] = {
    "varying vec2 imageCoord;"
    "uniform sampler2D image;"
    "void main() {"
        "vec2 p = imageCoord*128.0+0.5;"
        "vec2 i = floor(p);"
        "vec2 f = p-i;"
        "f = f*f*f*(f*(f*6.0-15.0)+10.0);"
        "p = i+f;"
        "p = (p-0.5)/128.0;"
        "gl_FragColor = texture2D(image, p);"
    "}"
};

const float FrontendCube::SIZE;


void FrontendCube::init(Cube::Hardware *_hw, b2World &world, float x, float y)
{
    hw = _hw;
    texture = 0;

    tiltTarget.Set(0,0);
    tiltVector.Set(0,0);

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
    bodyDef.userData = this;
    body = world.CreateBody(&bodyDef);

    b2PolygonShape box;
    const float boxSize = SIZE * 0.96;    // Compensate for polygon 'skin'
    box.SetAsBox(boxSize, boxSize);
    
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 0.8f;
    body->CreateFixture(&fixtureDef);
}

void FrontendCube::initGL()
{
    lcdVP = glCreateShaderObjectARB(GL_VERTEX_SHADER);
    glShaderSourceARB(lcdVP, 1, &srcLcdVP[0], NULL);
    glCompileShaderARB(lcdVP);

    lcdFP = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
    glShaderSourceARB(lcdFP, 1, &srcLcdFP[0], NULL);
    glCompileShaderARB(lcdFP);

    lcdProgram = glCreateProgramObjectARB();
    glAttachObjectARB(lcdProgram, lcdVP);
    glAttachObjectARB(lcdProgram, lcdFP);
    glLinkProgramARB(lcdProgram);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, Cube::LCD::WIDTH, Cube::LCD::HEIGHT,
                 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

 void FrontendCube::draw()
{
    /*
     * XXX: Crappy rounded body corners on a flat black
     *      polygon. Replace this with something beautiful later, like
     *      a real-time glossy reflection shader :)
     */
    const float height = HEIGHT;
    const float round = 0.05;
    const float epsilon = 0.01;

    static const GLfloat vaSides[] = {
        -1+round, -1,       0,
        -1+round, -1,       height,
         1-round, -1,       0,
         1-round, -1,       height,
         1,       -1+round, 0,
         1,       -1+round, height,
         1,        1-round, 0,
         1,        1-round, height,
         1-round,  1,       0,
         1-round,  1,       height,
        -1+round,  1,       0,
        -1+round,  1,       height,
        -1,        1-round, 0,
        -1,        1-round, height,
        -1,       -1+round, 0,
        -1,       -1+round, height,
        -1+round, -1,       0,
        -1+round, -1,       height,
    };

    static const GLfloat vaTop[] = {
        -1,       -1+round, height,
        -1,        1-round, height,
        -1+round,  1,       height,
         1-round,  1,       height,
         1,        1-round, height,
         1,       -1+round, height,
         1-round, -1,       height,
        -1+round, -1,       height,
    };

    static const GLfloat vaLcd[] = {
        0, 1,  -1,  1, height + epsilon,
        1, 1,   1,  1, height + epsilon,
        0, 0,  -1, -1, height + epsilon,
        1, 0,   1, -1, height + epsilon,
    };

    /*
     * Transforms.
     *
     * The whole modelview matrix is ours to set up as a cube->world
     * transform. We use this for rendering, and we also stow it for
     * use in converting acceleration data back to cube coordinates.
     */

    glLoadIdentity();

    b2Vec2 position = body->GetPosition();
    glTranslatef(position.x, position.y, 0);

    float32 angle = body->GetAngle() * (180 / M_PI);
    glRotatef(angle, 0,0,1);      

    const float tiltDeadzone = 5.0f;

    if (tiltVector.x > tiltDeadzone) {
        glTranslatef(SIZE, 0, height * SIZE);
        glRotatef(tiltVector.x - tiltDeadzone, 0,1,0);
        glTranslatef(-SIZE, 0, -height * SIZE);
    }
    if (tiltVector.x < -tiltDeadzone) {
        glTranslatef(-SIZE, 0, height * SIZE);
        glRotatef(tiltVector.x + tiltDeadzone, 0,1,0);
        glTranslatef(SIZE, 0, -height * SIZE);
    }

    if (tiltVector.y > tiltDeadzone) {
        glTranslatef(0, SIZE, height * SIZE);
        glRotatef(-tiltVector.y + tiltDeadzone, 1,0,0);
        glTranslatef(0, -SIZE, -height * SIZE);
    }
    if (tiltVector.y < -tiltDeadzone) {
        glTranslatef(0, -SIZE, height * SIZE);
        glRotatef(-tiltVector.y - tiltDeadzone, 1,0,0);
        glTranslatef(0, SIZE, -height * SIZE);
    }

    /* Save a copy of the transformation, before scaling it by our size. */
    GLfloat mat[16];
    glGetFloatv(GL_MODELVIEW, mat);
    modelMatrix.ex.x = mat[0];
    modelMatrix.ex.y = mat[1];
    modelMatrix.ex.z = mat[2];
    modelMatrix.ey.x = mat[4];
    modelMatrix.ey.y = mat[5];
    modelMatrix.ey.z = mat[6];
    modelMatrix.ez.x = mat[8];
    modelMatrix.ez.y = mat[9];
    modelMatrix.ez.z = mat[10];

    /* Now scale it */
    glScalef(SIZE, SIZE, SIZE);

    /*
     * Draw body
     */

    glUseProgramObjectARB(0);
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.9, 0.9, 0.9);
    glInterleavedArrays(GL_V3F, 0, vaSides);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 18);

    glColor3f(0.0, 0.0, 0.0);
    glInterleavedArrays(GL_V3F, 0, vaTop);
    glDrawArrays(GL_POLYGON, 0, 8);


    /*
     * Draw LCD image
     */

    if (hw->lcd.isVisible()) {
        // LCD on, draw the image with a shader

        glColor3f(1.0, 1.0, 1.0);
        glUseProgramObjectARB(lcdProgram);
        glEnable(GL_TEXTURE_2D);
        glUniform1iARB(glGetUniformLocationARB(lcdProgram, "image"), 0);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        Cube::LCD::WIDTH, Cube::LCD::HEIGHT,
                        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, hw->lcd.fb_mem);   
    } else {
        // LCD off, show a blank dark screen

        glColor3f(0.1, 0.1, 0.1);
        glUseProgramObjectARB(0);
        glDisable(GL_TEXTURE_2D);
    }

    glScalef(LCD_SIZE, LCD_SIZE, 1.0f);
    glInterleavedArrays(GL_T2F_V3F, 0, vaLcd);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

FrontendCube *FrontendCube::fromBody(b2Body *body)
{
    /*
     * Currently we don't have to do any type checking, since FrontendCubes
     * are the only kind of userdata we have in our scene.
     */
    return static_cast<FrontendCube *>(body->GetUserData());
}

void FrontendCube::setTiltTarget(b2Vec2 angles)
{
    /* Target tilt angles that we're animating toward, as YX Euler angles in degrees */
    tiltTarget = angles;
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

    b2Vec3 accelG = accel.measure(body, 0.1f);

    /*
     * Now use the same matrix we computed while rendering the last frame, and
     * convert this acceleration into device-local coordinates. This will take into
     * account tilting, as well as the cube's angular orientation.
     */

    b2Vec3 accelLocal = modelMatrix.Solve33(accelG);

    /*
     * Now we have acceleration, in G's, in the same coordinate system
     * used by the accelerometer. Scale it according to the
     * accelerometer's maximum range (ours is rated at +/- 2G) and
     * send it to the hardware emulation.
     */

    const float deviceAccelScale = 128.0 / 2.0;

    b2Vec2 deviceAccel(accelLocal.x * deviceAccelScale,
                       accelLocal.y * deviceAccelScale);
    int8_t accelX = b2Clamp((int)deviceAccel.x, -128, 127); 
    int8_t accelY = b2Clamp((int)deviceAccel.y, -128, 127); 

    hw->i2c.accel.setVector(accelX, accelY);
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
