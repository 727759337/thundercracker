/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
using namespace Sifteo;

static Metadata M = Metadata()
    .title("TinySynth")
    .package("com.sifteo.sdk.synth", "1.0")
    .cubeRange(1);

static const CubeID cube = 0;
static VideoBuffer vid;

const AudioChannel squareChannel(0);
const AudioChannel sine1Channel(1);
const AudioChannel sine2Channel(2);

static int16_t sineWave[64];
static const AssetAudio sineAsset = AssetAudio::fromPCM(sineWave);

static int16_t squareWave[256];
static const AssetAudio squareAsset = AssetAudio::fromPCM(squareWave);

void synthInit()
{
    for (int i = 0; i != arraysize(sineWave); i++) {
        float theta = i * float(M_PI * 2 / (arraysize(sineWave)-1));
        sineWave[i] = sin(theta) * 0x7fff;
    }

    sine1Channel.play(sineAsset);
    sine2Channel.play(sineAsset);
    squareChannel.play(squareAsset);
}

void synthesize(float hz, float timbre, float volume)
{
    LOG("hz=%f timbre=%f volume=%f\n", hz, timbre, volume);
    
    int dutyCycle = timbre * arraysize(squareWave);
    for (int i = 0; i != arraysize(squareWave); i++) {
        squareWave[i] = i < dutyCycle ? 0x7fff : 0x8000;
    }

    sine1Channel.setVolume(volume * 128.f);
    sine2Channel.setVolume(volume * 96.f);
    squareChannel.setVolume(volume * 32.f);

    sine1Channel.setSpeed(hz * arraysize(sineWave));                // Fundamental
    sine2Channel.setSpeed(hz * 1.02f * arraysize(sineWave));        // Beat frequency
    squareChannel.setSpeed(hz * 1.26f * arraysize(squareWave));     // 5 half-steps above
}

void main()
{
    unsigned fg = BG0ROMDrawable::SOLID_FG ^ BG0ROMDrawable::LTBLUE;
    unsigned bg = BG0ROMDrawable::SOLID_FG ^ BG0ROMDrawable::BLACK;
    
    vid.initMode(BG0_ROM);
    vid.bg0rom.erase(bg);
    vid.bg0rom.fill(vec(0,0), vec(3,3), fg);
    vid.attach(cube);
    synthInit();

    float hz = 0;

    while (1) {
        // Scale to [-1, 1]
        auto accel = cube.accel() / 128.f;

        // Glide to the target note (half-steps above or below middle C)
        float note = 261.6f * pow(1.05946f, round(accel.y * 24.f));
        hz += (note - hz) * 0.4f;

        synthesize(hz, accel.x,
            clamp(accel.x + 0.5f, 0.f, 1.f));

        const Int2 center = LCD_center - vec(24,24)/2;
        vid.bg0rom.setPanning(-(center + accel.xy() * 60.f));
        System::paint();
    }
}
