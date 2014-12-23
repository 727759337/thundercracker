/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _GL_RENDERER_H
#define _GL_RENDERER_H

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#ifdef __MACH__
#   define HAS_GL_ENTRY(x)   (true)
#   include <OpenGL/gl.h>
#   include <OpenGL/glext.h>
#elif defined (_WIN32)
#   define GLEW_STATIC
#   define HAS_GL_ENTRY(x)   ((x) != NULL)
#   include <GL/glew.h>
#else
#   define GL_GLEXT_PROTOTYPES
#   define HAS_GL_ENTRY(x)   (true)
#   include <GL/gl.h>
#   include <GL/glext.h>
#endif

#ifndef GL_UNSIGNED_SHORT_5_6_5
#   define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif

#include <Box2D/Box2D.h>
#include <vector>
#include <string>
#include "system.h"
#include "frontend_model.h"


class GLRenderer {
 public:
    bool init();
    void setViewport(int width, int height);

    void beginFrame(float viewExtent, b2Vec2 viewCenter, unsigned pixelZoomMode=0);
    static void endFrame();

    void drawDefaultBackground(float extent, float scale);
    void drawSolidBackground(const float color[4]);

    void drawCube(unsigned id, b2Vec2 center, float angle, float hover,
                  b2Vec2 tilt, const uint16_t *framebuffer, bool framebufferChanged,
                  float backlight, b2Mat33 &modelMatrix);
    void drawMC(b2Vec2 center, float angle, const float led[3], float volume);

    void beginOverlay();
    void overlayRect(int x, int y, int w, int h, const float color[4], GLhandleARB program = 0);
    void overlayRect(int x, int y, int w, int h);
    void overlayText(int x, int y, const float color[4], const char *str);
    int measureText(const char *str);
    void overlayCubeFlash(unsigned id, int x, int y, int w, int h,
        const uint8_t *data, bool dataChanged);
    void overlayAudioVisualizer(float alpha);

    void takeScreenshot(std::string name) {
        // Screenshots are asynchronous
        pendingScreenshotName = name;
    }
    
    unsigned getWidth() const {
        return viewportWidth;
    }
    
    unsigned getHeight() const {
        return viewportHeight;
    }

 private:

    struct CubeTransformState {
        b2Mat33 *modelMatrix;
        bool isTilted;
        bool nonPixelAccurate;
    };
    
    struct Model {
        FrontendModel data;
        GLuint vb;
        GLuint ib;
    };

    void initCubeFB(unsigned id);
    void initCubeTexture(GLuint name, bool pixelAccurate);
    void cubeTransform(b2Vec2 center, float angle, float hover,
                       b2Vec2 tilt, CubeTransformState &tState);

    void drawCubeBody();
    void drawCubeFace(unsigned id, const uint16_t *framebuffer, float backlight);

    void loadModel(const uint8_t *data, Model &model);
    void drawModel(Model &model);
    
    GLuint loadTexture(const uint8_t *pngData, GLenum wrap=GL_CLAMP, GLenum filter=GL_LINEAR);
    GLhandleARB loadShader(GLenum type, const uint8_t *source, const char *prefix="");
    GLhandleARB linkProgram(GLhandleARB fp, GLhandleARB vp);
    GLhandleARB loadCubeFaceProgram(const char *prefix);
    
    void saveTexturePNG(std::string name, unsigned width, unsigned height);
    void saveColorBufferPNG(std::string name);

    int viewportWidth, viewportHeight;

    Model cubeBody;
    Model cubeFace;
    Model mcBody;
    Model mcFace;
    Model mcVolume;

    GLhandleARB cubeFaceProgFiltered;
    GLhandleARB cubeFaceProgUnfiltered;
    GLuint cubeFaceHilightTexture;

    GLhandleARB cubeBodyProgram;

    GLhandleARB mcFaceProgram;
    GLuint mcFaceNormalsTexture;
    GLuint mcFaceLightTexture;
    GLuint mcFaceLEDLocation;

    GLhandleARB backgroundProgram;
    GLuint backgroundTexture;
    GLuint bgLightTexture;
    GLuint logoTexture;

    GLhandleARB scopeProgram;
    GLint scopeAlphaAttr;
    GLuint scopeSampleTexture;
    GLuint scopeBackgroundTexture;
    
    GLuint fontTexture;
    
    struct {
        float viewExtent;
        unsigned pixelZoomMode;
    } currentFrame;

    std::vector<VertexT> overlayVA;

    // To reduce graphics pipeline stalls, we have a small round-robin buffer of textures.
    static const unsigned NUM_LCD_TEXTURES = 4;
    
    struct GLCube {
        bool fbInitialized;
        bool flashInitialized;
        bool pixelAccurate;
        bool isTilted;
        uint8_t currentLcdTexture;
        GLuint texFiltered[NUM_LCD_TEXTURES];
        GLuint texAccurate[NUM_LCD_TEXTURES];
        GLuint flashTex;
    } cubes[System::MAX_CUBES];

    std::string pendingScreenshotName;
};

#endif
