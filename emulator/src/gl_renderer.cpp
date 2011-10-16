/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "gl_renderer.h"
#include "frontend.h"
#include "lodepng.h"


bool GLRenderer::init()
{
#ifdef GLEW_STATIC
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return false;
    }
#endif

    if (!HAS_GL_ENTRY(glCreateShaderObjectARB)) {
        fprintf(stderr, "Error: Shader support not available\n");
        return false;
    }
        
    /*
     * Compile our shaders
     *
     * About our shader for rendering the LCD... We could probably do
     * something more complicated with simulating the subpixels in the
     * LCD screens, but right now I just have two goals:
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

    extern const uint8_t cube_face_fp[];
    extern const uint8_t cube_face_vp[];
    GLhandleARB cubeFaceFP = loadShader(GL_FRAGMENT_SHADER, cube_face_fp);
    GLhandleARB cubeFaceVP = loadShader(GL_VERTEX_SHADER, cube_face_vp);
    cubeFaceProgram = linkProgram(cubeFaceFP, cubeFaceVP);

    glUseProgramObjectARB(cubeFaceProgram);
    glUniform1iARB(glGetUniformLocationARB(cubeFaceProgram, "face"), 0);
    glUniform1iARB(glGetUniformLocationARB(cubeFaceProgram, "hilight"), 1);
    glUniform1iARB(glGetUniformLocationARB(cubeFaceProgram, "mask"), 2);
    glUniform1iARB(glGetUniformLocationARB(cubeFaceProgram, "lcd"), 3);    

    extern const uint8_t cube_side_fp[];
    extern const uint8_t cube_side_vp[];
    GLhandleARB cubeSideFP = loadShader(GL_FRAGMENT_SHADER, cube_side_fp);
    GLhandleARB cubeSideVP = loadShader(GL_VERTEX_SHADER, cube_side_vp);
    cubeSideProgram = linkProgram(cubeSideFP, cubeSideVP);

    extern const uint8_t background_fp[];
    extern const uint8_t background_vp[];
    GLhandleARB backgroundFP = loadShader(GL_FRAGMENT_SHADER, background_fp);
    GLhandleARB backgroundVP = loadShader(GL_VERTEX_SHADER, background_vp);
    backgroundProgram = linkProgram(backgroundFP, backgroundVP);

    glUseProgramObjectARB(backgroundProgram);
    glUniform1iARB(glGetUniformLocationARB(backgroundProgram, "texture"), 0);

    /*
     * Load textures
     */

    extern const uint8_t img_cube_face[];
    extern const uint8_t img_cube_face_hilight[];
    extern const uint8_t img_cube_face_hilight_mask[];
    extern const uint8_t img_wood[];
    extern const uint8_t ui_font_data_0[];

    cubeFaceTexture = loadTexture(img_cube_face);
    cubeFaceHilightTexture = loadTexture(img_cube_face_hilight);
    cubeFaceHilightMaskTexture = loadTexture(img_cube_face_hilight_mask);
    backgroundTexture = loadTexture(img_wood, GL_REPEAT);
    fontTexture = loadTexture(ui_font_data_0, GL_CLAMP, GL_NEAREST);
    
    /*
     * Procedural models
     */

    createRoundedRect(faceVA, 1.0f, FrontendCube::HEIGHT, 0.242f);
    extrudePolygon(faceVA, sidesVA);

    /*
     * Per-cube initialization is lazy
     */

    for (unsigned i = 0; i < System::MAX_CUBES; i++)
        cubes[i].initialized = false;

    return true;
}

GLhandleARB GLRenderer::loadShader(GLenum type, const uint8_t *source)
{
    const GLchar *stringArray[1];
    stringArray[0] = (const GLchar *) source;

    GLhandleARB shader = glCreateShaderObjectARB(type);
    glShaderSourceARB(shader, 1, stringArray, NULL);
    glCompileShaderARB(shader);

    GLint isCompiled = false;
    glGetObjectParameterivARB(shader, GL_COMPILE_STATUS, &isCompiled);
    if (!isCompiled) {
        GLchar log[1024];
        GLint logLength = sizeof log;

        glGetInfoLogARB(shader, logLength, &logLength, log);
        log[logLength] = '\0';

        fprintf(stderr, "Error: Shader compile failed.\n%s", log);
    }

    return shader;
}

GLhandleARB GLRenderer::linkProgram(GLhandleARB fp, GLhandleARB vp)
{
    GLhandleARB program = glCreateProgramObjectARB();
    glAttachObjectARB(program, vp);
    glAttachObjectARB(program, fp);
    glLinkProgramARB(program);

    GLint isLinked = false;
    glGetObjectParameterivARB(program, GL_LINK_STATUS, &isLinked);
    if (!isLinked) {
        GLchar log[1024];
        GLint logLength = sizeof log;

        glGetInfoLogARB(program, logLength, &logLength, log);
        log[logLength] = '\0';

        fprintf(stderr, "Error: Shader link failed.\n%s", log);
    }

    return program;
}

void GLRenderer::setViewport(int width, int height)
{
    glViewport(0, 0, width, height);
    viewportWidth = width;
    viewportHeight = height;
}

void GLRenderer::beginFrame(float viewExtent, b2Vec2 viewCenter)
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
    float zFar = 10.0f;
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
    glTranslatef(-viewCenter.x, -viewCenter.y, -zCamera);
    glScalef(1, 1, 1.0 / viewExtent);
    glTranslatef(0, 0, -zPlane);
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    glClear(GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::endFrame()
{
    glfwSwapBuffers();
}

void GLRenderer::beginOverlay()
{
    /*
     * Handle screenshots here, since we don't want
     * the screenshot to include overlay text. Save the
     * full screenshot, and end our pending screenshot request.
     */
    if (!pendingScreenshotName.empty()) {
                saveColorBufferPNG(pendingScreenshotName + ".png");
                pendingScreenshotName.clear();
        }       

    // Do our text rendering in pixel coordinates
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1, -1, 1);
    glTranslatef(-1, -1, 0);
    glScalef(2.0f / viewportWidth, 2.0f / viewportHeight, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);    
}

unsigned GLRenderer::measureText(const char *str)
{
    unsigned x = 0, w = 0;
    uint32_t id;
    
    while ((id = *(str++))) {
        const Glyph *g = findGlyph(id);
        if (g) {
            w = MAX(w, x + g->xOffset + g->width);
            x += g->xAdvance;
        }
    }
    
    return w;
}

void GLRenderer::overlayText(unsigned x, unsigned y, const float color[4], const char *str)
{
    const float TEXTURE_WIDTH = 128.0f;
    const float TEXTURE_HEIGHT = 256.0f;
   
    overlayVA.clear();

    uint32_t id;
    while ((id = *(str++))) {
        const Glyph *g = findGlyph(id);
        if (g) {
            VertexT a, b, c, d;
            
            a.tx = g->x / TEXTURE_WIDTH;
            a.ty = g->y / TEXTURE_HEIGHT;
            a.vx = x + g->xOffset;
            a.vy = y + g->yOffset;
            a.vz = 0;
            
            b = a;
            b.tx += g->width / TEXTURE_WIDTH;
            b.vx += g->width;
            
            d = a;
            d.ty += g->height / TEXTURE_HEIGHT;
            d.vy += g->height;
            
            c = b;
            c.ty = d.ty;
            c.vy = d.vy;
            
            overlayVA.push_back(a);
            overlayVA.push_back(b);
            overlayVA.push_back(c);
            
            overlayVA.push_back(a);
            overlayVA.push_back(c);
            overlayVA.push_back(d);
        
            x += g->xAdvance;
        }
    }

    glUseProgramObjectARB(0);
    glColor4fv(color);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glInterleavedArrays(GL_T2F_V3F, 0, &overlayVA[0]);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei) overlayVA.size());
    glDisable(GL_TEXTURE_2D);
}         

void GLRenderer::overlayRect(unsigned x, unsigned y,
                             unsigned w, unsigned h, const float color[4])
{
    overlayVA.clear();
    VertexT a, b, c, d;
           
    a.tx = 0;
    a.ty = 0;
    a.vx = x;
    a.vy = y;
    a.vz = 0;
            
    b = a;
    b.vx += w;
    
    d = a;
    d.vy += h;
    
    c = b;
    c.vy = d.vy;
            
    overlayVA.push_back(a);
    overlayVA.push_back(b);
    overlayVA.push_back(c);
         
    overlayVA.push_back(a);
    overlayVA.push_back(c);
    overlayVA.push_back(d);
        
    glUseProgramObjectARB(0);
    glColor4fv(color);
    glInterleavedArrays(GL_T2F_V3F, 0, &overlayVA[0]);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei) overlayVA.size());
}
                    
void GLRenderer::drawBackground(float extent, float scale)
{
    float tc = scale * extent;
    VertexTN bg[] = {
        {-tc,  tc,   0.0f, 0.0f, 1.0f,   -extent,  extent, 0.0f },
        { tc,  tc,   0.0f, 0.0f, 1.0f,    extent,  extent, 0.0f },
        {-tc, -tc,   0.0f, 0.0f, 1.0f,   -extent, -extent, 0.0f },
        { tc, -tc,   0.0f, 0.0f, 1.0f,    extent, -extent, 0.0f },
    };

    glLoadIdentity();
    glUseProgramObjectARB(backgroundProgram);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);

    glInterleavedArrays(GL_T2F_N3F_V3F, 0, bg);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(GL_TEXTURE_2D);
}

void GLRenderer::initCube(unsigned id)
{
    cubes[id].initialized = true;

    glGenTextures(1, &cubes[id].lcdTexture);
    glBindTexture(GL_TEXTURE_2D, cubes[id].lcdTexture);
        
    glTexImage2D(GL_TEXTURE_2D, 0, 3, Cube::LCD::WIDTH, Cube::LCD::HEIGHT,
                 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
        
    /*
     * We rely on mipmaps (with autogeneration) for good-looking
     * minification, and our nonlinear texture sampling (in the
     * fragment program) for good-looking rotation and magnification.
     */

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 5.0f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
}

void GLRenderer::cubeTransform(b2Vec2 center, float angle, float hover,
                               b2Vec2 tilt, b2Mat33 &modelMatrix)
{
    /*
     * The whole modelview matrix is ours to set up as a cube->world
     * transform. We use this for rendering, and we also stow it for
     * use in converting acceleration data back to cube coordinates.
     */

    glLoadIdentity();
    glTranslatef(center.x, center.y, 0.0f);
    glRotatef(angle * (180.0f / M_PI), 0,0,1);      

    const float tiltDeadzone = 5.0f;
    const float height = FrontendCube::HEIGHT;

    if (tilt.x > tiltDeadzone) {
        glTranslatef(FrontendCube::SIZE, 0, height * FrontendCube::SIZE);
        glRotatef(tilt.x - tiltDeadzone, 0,1,0);
        glTranslatef(-FrontendCube::SIZE, 0, -height * FrontendCube::SIZE);
    }
    if (tilt.x < -tiltDeadzone) {
        glTranslatef(-FrontendCube::SIZE, 0, height * FrontendCube::SIZE);
        glRotatef(tilt.x + tiltDeadzone, 0,1,0);
        glTranslatef(FrontendCube::SIZE, 0, -height * FrontendCube::SIZE);
    }

    if (tilt.y > tiltDeadzone) {
        glTranslatef(0, FrontendCube::SIZE, height * FrontendCube::SIZE);
        glRotatef(-tilt.y + tiltDeadzone, 1,0,0);
        glTranslatef(0, -FrontendCube::SIZE, -height * FrontendCube::SIZE);
    }
    if (tilt.y < -tiltDeadzone) {
        glTranslatef(0, -FrontendCube::SIZE, height * FrontendCube::SIZE);
        glRotatef(-tilt.y - tiltDeadzone, 1,0,0);
        glTranslatef(0, FrontendCube::SIZE, -height * FrontendCube::SIZE);
    }

    /* Save a copy of the transformation, before scaling it by our size. */
    GLfloat mat[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mat);
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
    glScalef(FrontendCube::SIZE, FrontendCube::SIZE, FrontendCube::SIZE);
    
    /* Hover is relative to cube size, so apply that now. */
    glTranslatef(0.0f, 0.0f, hover);
}


void GLRenderer::drawCube(unsigned id, b2Vec2 center, float angle, float hover,
                          b2Vec2 tilt, uint16_t *framebuffer, b2Mat33 &modelMatrix)
{
    if (!cubes[id].initialized)
        initCube(id);

    cubeTransform(center, angle, hover, tilt, modelMatrix);

    drawCubeBody();
    drawCubeFace(id, framebuffer);
}

void GLRenderer::drawCubeBody()
{
    glUseProgramObjectARB(cubeSideProgram);
    glInterleavedArrays(GL_T2F_N3F_V3F, 0, &sidesVA[0]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei) sidesVA.size());
}

void GLRenderer::drawCubeFace(unsigned id, uint16_t *framebuffer)
{
    static const uint16_t blackness[Cube::LCD::WIDTH * Cube::LCD::HEIGHT] = { 0 };

    glUseProgramObjectARB(cubeFaceProgram);

    /*
     * Set up samplers
     */

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cubeFaceTexture);

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cubeFaceHilightTexture);

    glActiveTexture(GL_TEXTURE2);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cubeFaceHilightMaskTexture);
    
    glActiveTexture(GL_TEXTURE3);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cubes[id].lcdTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    Cube::LCD::WIDTH, Cube::LCD::HEIGHT,
                    GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                    framebuffer ? framebuffer : blackness);

        if (!pendingScreenshotName.empty()) {
                // Opportunistically grab this texture for a screenshot
        char suffix[32];
        snprintf(suffix, sizeof suffix, "-cube%02d.png", id);
        saveTexturePNG(pendingScreenshotName + std::string(suffix),
                       Cube::LCD::WIDTH, Cube::LCD::HEIGHT);
        }

    /*
     * Just one draw. Our shader handles the rest. Drawing the LCD and
     * face with one shader gives us a chance to do some nice
     * filtering on the edges of the LCD.
     */

    glInterleavedArrays(GL_T2F_N3F_V3F, 0, &faceVA[0]);
    glDrawArrays(GL_POLYGON, 0, (GLsizei) faceVA.size());

    /*
     * Clean up GL state.
     */

    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE2);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE3);
    glDisable(GL_TEXTURE_2D);
}

GLuint GLRenderer::loadTexture(const uint8_t *pngData, GLenum wrap, GLenum filter)
{
    LodePNG::Decoder decoder;
    std::vector<unsigned char> pixels;

    // This is trusted data, no need for an explicit size limit.
    decoder.decode(pixels, pngData, 0x10000000);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 decoder.getWidth(),
                 decoder.getHeight(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 &pixels[0]);

    // Sane defaults
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    
    return texture;
}

void GLRenderer::createRoundedRect(std::vector<GLRenderer::VertexTN> &outPolygon,
                                   float size, float height, float relRadius)
{
    const float step = M_PI / 32.0f;
    float side = 1.0 - relRadius;

    for (float angle = M_PI*2; angle > 0.0f; angle -= step) {
        VertexTN v;

        float x = cosf(angle) * relRadius;
        float y = sinf(angle) * relRadius;

        if (x < 0)
            x -= side;
        else
            x += side;

        if (y < 0)
            y -= side;
        else
            y += side;

        v.tx = (x + 1.0f) * 0.5f;
        v.ty = (y + 1.0f) * 0.5f;

        v.nx = 0.0f;
        v.ny = 0.0f;
        v.nz = 1.0f;

        v.vx = x * size;
        v.vy = y * size;
        v.vz = height;

        outPolygon.push_back(v);
    }
}

void GLRenderer::extrudePolygon(const std::vector<GLRenderer::VertexTN> &inPolygon,
                                std::vector<GLRenderer::VertexTN> &outTristrip)
{
    const VertexTN *prev = &inPolygon[inPolygon.size() - 1];

    for (std::vector<VertexTN>::const_iterator i = inPolygon.begin();
         i != inPolygon.end(); i++) {
        const VertexTN *current = &*i;
        VertexTN a, b;
        
        a = *current;

        // Cross product (simplified)
        a.nx = current->vy - prev->vy;
        a.ny = current->vx - prev->vx;
        a.nz = 0;

        // Normalize
        float n = sqrtf(a.nx * a.nx + a.ny * a.ny);
        a.nx /= n;
        a.ny /= n;

        b = a;
        b.vz = 0;

        outTristrip.push_back(a);
        outTristrip.push_back(b);
        prev = current;
    }
}

void GLRenderer::saveTexturePNG(std::string name, unsigned width, unsigned height)
{
    std::vector<uint8_t> pixels(width * height * 4, 0);
    std::vector<uint8_t> png;
    LodePNG::Encoder encoder;
    
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

    encoder.getInfoPng().color.colorType = LCT_RGB;
    encoder.encode(png, pixels, width, height);
    
    LodePNG::saveFile(png, name);
}

void GLRenderer::saveColorBufferPNG(std::string name)
{
    unsigned width = viewportWidth;
    unsigned height = viewportHeight;
    unsigned stride = width * 4;
    std::vector<uint8_t> pixels(stride * height, 0);
    std::vector<uint8_t> swappedPixels(stride * height, 0);
    std::vector<uint8_t> png;
    LodePNG::Encoder encoder;
    
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
    
    // Swap lines, bottom-up to top-down       
    for (unsigned i = 0; i < height; i++)
        memcpy(&swappedPixels[stride * (height - i - 1)],
               &pixels[stride * i], stride);
    
    encoder.getInfoPng().color.colorType = LCT_RGB;
    encoder.encode(png, swappedPixels, width, height);
    
    LodePNG::saveFile(png, name);
}

const GLRenderer::Glyph *GLRenderer::findGlyph(uint32_t id)
{
    /*
     * Very simplistic- linear search, one font, one texture page.
     * Nothing fancy going on. The BMFont format is pretty simple to
     * begin with, and here we're ignoring most of it.
     *
     * This is also assuming we're on a little-endian CPU that's fine
     * with unaligned accesses.
     */
     
    struct __attribute__ ((packed)) Header {
        uint8_t type;
        uint32_t size;
    };

    extern const uint8_t ui_font_data[];
    const uint8_t *p = ui_font_data + 4;
    const Header *hdr;
    
    // Find the 'chars' block (type 4)
    do {
        hdr = (const Header *)p;
        p += sizeof *hdr + hdr->size;
    } while (hdr->type != 4);
    
    // Find the character we're looking for (linear scan)
    const Glyph *chars = (const Glyph *) (hdr + 1);
    while (chars < (const Glyph *) p) {
        if (chars->id == id)
            return chars;
        chars++;
    }

    return NULL;
}
