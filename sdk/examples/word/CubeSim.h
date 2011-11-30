#ifndef CUBESIM_H
#define CUBESIM_H

#define NUM_CUBES 3

struct Float2 {
    float x, y;
};

class CubeSim {
public:

    static const unsigned numStars = 8;

    static const float textSpeed = 0.2f;
    static const float bgScrollSpeed = 10.0f;
    static const float bgTiltSpeed = 1.0f;
    static const float starEmitSpeed = 0.3f;
    static const float starTiltSpeed = 3.0f;

    CubeSim(Cube &cube)
        : cube(cube)
    {}

    void init()
    {
        frame = 0;
        bg.x = 0;
        bg.y = 0;

        for (unsigned i = 0; i < numStars; i++)
            initStar(i);

        VidMode_BG0 vid(cube.vbuf);
        vid.init();
        vid.BG0_drawAsset(Vec2(0,0), Background);

        // Clear BG1/SPR before switching modes
        _SYS_vbuf_fill(&cube.vbuf.sys, _SYS_VA_BG1_BITMAP/2, 0, 16);
        _SYS_vbuf_fill(&cube.vbuf.sys, _SYS_VA_BG1_TILES/2,
                       cube.vbuf.indexWord(Font.index), 32);
        _SYS_vbuf_fill(&cube.vbuf.sys, _SYS_VA_SPR, 0, 8*5/2);

        // Allocate 16x2 tiles on BG1 for text at the bottom of the screen
        _SYS_vbuf_fill(&cube.vbuf.sys, _SYS_VA_BG1_BITMAP/2 + 14, 0xFFFF, 2);

        // Switch modes
        _SYS_vbuf_pokeb(&cube.vbuf.sys, offsetof(_SYSVideoRAM, mode), _SYS_VM_BG0_SPR_BG1);
    }

    void update(float timeStep)
    {
        /*
         * Action triggers
         */

        switch (frame) {

        case 0:
            text.x = 0;
            text.y = -20;
            textTarget = text;
            break;

        case 100:
            writeText(" Word Game WIP ");
            textTarget.y = 8;
            break;

        case 200:
            textTarget.y = -20;
            break;

        case 250:
            writeText(" Tilt me to fly ");
            textTarget.y = 8;
            break;

        case 350:
            textTarget.y = -20;
            break;

        case 400:
            writeText(" Enjoy subspace ");
            textTarget.y = 64-8;
            break;

        case 500:
            textTarget.x = 128;
            break;

        case 550:
            text.x = 128;
            textTarget.x = 0;
            writeText(" and be careful ");
            break;

        case 650:
            textTarget.y = -20;
            break;

        case 800: {
            text.x = -4;
            text.y = 128;
            textTarget.x = -4;
            textTarget.y = 128-16;

            /*
             * This is the *average* FPS so far, as measured by the game.
             * If the cubes aren't all running at the same frame rate, this
             * will be roughly the rate of the slowest cube, due to how the
             * flow control in System::paint() works.
             */

             char buf[17];
            float fps = frame / fpsTimespan;
            snprintf(buf, sizeof buf, "%d.%02d FPS avg        ",
                    (int)fps, (int)(fps * 100) % 100);
            buf[16] = 0;
            writeText(buf);
            break;
        }

        case 900:
            textTarget.y = 128;
            break;

        case 2048:
            frame = 0;
            fpsTimespan = 0;
            break;
        }

        /*
         * We respond to the accelerometer
         */

        _SYSAccelState accel;
        _SYS_getAccel(cube.id(), &accel);

        /*
         * Update starfield animation
         */

        for (unsigned i = 0; i < numStars; i++) {
            resizeSprite(i, 8, 8);
            setSpriteImage(i, Star.index + (frame % Star.frames) * Star.width * Star.height);
            moveSprite(i, stars[i].x + (64 - 3.5f), stars[i].y + (64 - 3.5f));

            stars[i].x += timeStep * (stars[i].vx + accel.x * starTiltSpeed);
            stars[i].y += timeStep * (stars[i].vy + accel.y * starTiltSpeed);

            if (stars[i].x > 80 || stars[i].x < -80 ||
                stars[i].y > 80 || stars[i].y < -80)
                initStar(i);
        }

        /*
         * Update global animation
         */

        frame++;
        fpsTimespan += timeStep;

        VidMode_BG0 vid(cube.vbuf);
        bg.x += timeStep * (accel.x * bgTiltSpeed);
        bg.y += timeStep * (accel.y * bgTiltSpeed - bgScrollSpeed);
        vid.BG0_setPanning(Vec2(bg.x + 0.5f, bg.y + 0.5f));

        text.x += (textTarget.x - text.x) * textSpeed;
        text.y += (textTarget.y - text.y) * textSpeed;
        panBG1(text.x + 0.5f, text.y + 0.5f);
    }

private:
    struct {
        float x, y, vx, vy;
    } stars[numStars];

    Cube &cube;
    unsigned frame;
    Float2 bg, text, textTarget;

    float fpsTimespan;

    void moveSprite(int id, int x, int y)
    {
        uint8_t xb = -x;
        uint8_t yb = -y;

        uint16_t word = ((uint16_t)xb << 8) | yb;
        uint16_t addr = ( offsetof(_SYSVideoRAM, spr[0].pos_y)/2 +
                        sizeof(_SYSSpriteInfo)/2 * id );

        _SYS_vbuf_poke(&cube.vbuf.sys, addr, word);
    }

    void resizeSprite(int id, int w, int h)
    {
        uint8_t xb = -w;
        uint8_t yb = -h;

        uint16_t word = ((uint16_t)xb << 8) | yb;
        uint16_t addr = ( offsetof(_SYSVideoRAM, spr[0].mask_y)/2 +
                          sizeof(_SYSSpriteInfo)/2 * id );

        _SYS_vbuf_poke(&cube.vbuf.sys, addr, word);
    }

    void setSpriteImage(int id, int tile)
    {
        uint16_t word = VideoBuffer::indexWord(tile);
        uint16_t addr = ( offsetof(_SYSVideoRAM, spr[0].tile)/2 +
                          sizeof(_SYSSpriteInfo)/2 * id );

        _SYS_vbuf_poke(&cube.vbuf.sys, addr, word);
    }

    void panBG1(int8_t x, int8_t y)
    {
        _SYS_vbuf_poke(&cube.vbuf.sys, offsetof(_SYSVideoRAM, bg1_x) / 2,
                       ((uint8_t)x) | ((uint16_t)(uint8_t)y << 8));
    }

    void writeText(const char *str)
    {
        // Text on BG1, in the 16x2 area we allocated
        uint16_t addr = offsetof(_SYSVideoRAM, bg1_tiles) / 2;
        char c;

        while ((c = *(str++))) {
            uint16_t index = (c - ' ') * 2 + Font.index;
            cube.vbuf.pokei(addr, index);
            cube.vbuf.pokei(addr + 16, index + 1);
            addr++;
        }
    }

    void initStar(int id)
    {
        stars[id].x = 0;
        stars[id].y = 0;

        float angle = randint(1000) * (2 * M_PI / 1000.0f);
        float speed = (randint(200) + 50) * starEmitSpeed;

        stars[id].vx = cosf(angle) * speed;
        stars[id].vy = sinf(angle) * speed;
    }

    static unsigned int randint(unsigned int max)
    {
    #ifdef _WIN32
        return rand()%max;
    #else
        static unsigned int seed = (int)System::clock();
        return rand_r(&seed)%max;
    #endif
    }
};

#endif // CUBESIM_H
