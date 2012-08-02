/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

static Metadata M = Metadata()
    .title("Sensors SDK Example")
    .package("com.sifteo.sdk.sensors", "1.1")
    .icon(Icon)
    .cubeRange(0, CUBE_ALLOCATION);

static VideoBuffer vid[CUBE_ALLOCATION];


class SensorListener {
public:
    struct Counter {
        unsigned touch;
        unsigned shake;
        unsigned neighborAdd;
        unsigned neighborRemove;
    } counters[CUBE_ALLOCATION];

    void install()
    {
        Events::cubeTouch.set(&SensorListener::onTouch, this);
        Events::cubeShake.set(&SensorListener::onShake, this);
        Events::neighborAdd.set(&SensorListener::onNeighborAdd, this);
        Events::neighborRemove.set(&SensorListener::onNeighborRemove, this);
        Events::cubeAccelChange.set(&SensorListener::onAccelChange, this);
        Events::cubeBatteryLevelChange.set(&SensorListener::onBatteryChange, this);
        Events::cubeConnect.set(&SensorListener::onConnect, this);

        // Handle already-connected cubes
        for (CubeID cube : CubeSet::connected())
            onConnect(cube);
    }

private:
    void onConnect(unsigned id)
    {
        CubeID cube(id);
        uint64_t hwid = cube.hwID();

        bzero(counters[id]);
        LOG("Cube %d connected\n", id);

        vid[id].initMode(BG0_ROM);
        vid[id].attach(id);

        // Draw the cube's identity
        String<128> str;
        str << "I am cube #" << cube << "\n";
        str << "hwid " << Hex(hwid >> 32) << "\n     " << Hex(hwid) << "\n\n";
        vid[cube].bg0rom.text(vec(1,2), str);

        // Draw initial state for all sensors
        onTouch(cube);
        onShake(cube);
        onAccelChange(cube);
        onBatteryChange(cube);
        drawNeighbors(cube);
    }

    void onBatteryChange(unsigned id)
    {
        CubeID cube(id);
        String<32> str;
        str << "bat:   " << FixedFP(cube.batteryLevel(), 1, 3) << "\n";
        vid[cube].bg0rom.text(vec(1,13), str);
    }

    void onTouch(unsigned id)
    {
        CubeID cube(id);
        counters[id].touch++;
        LOG("Touch event on cube #%d, state=%d\n", id, cube.isTouching());

        String<32> str;
        str << "touch: " << cube.isTouching() <<
            " (" << counters[cube].touch << ")\n";
        vid[cube].bg0rom.text(vec(1,8), str);
    }

    void onShake(unsigned cube)
    {
        counters[cube].shake++;
        LOG("Shaking cube #%d\n", cube);

        String<16> str;
        str << "shake: " << counters[cube].shake;
        vid[cube].bg0rom.text(vec(1,12), str);
    }

    void onAccelChange(unsigned id)
    {
        CubeID cube(id);
        auto accel = cube.accel();

        String<64> str;
        str << "acc: "
            << Fixed(accel.x, 3)
            << Fixed(accel.y, 3)
            << Fixed(accel.z, 3) << "\n";

        auto tilt = cube.tilt();
        str << "tilt:"
            << Fixed(tilt.x, 3)
            << Fixed(tilt.y, 3)
            << Fixed(tilt.z, 3) << "\n";
        vid[cube].bg0rom.text(vec(1,10), str);
    }

    void onNeighborRemove(unsigned firstCube, unsigned firstSide,
        unsigned secondCube, unsigned secondSide)
    {
        counters[firstCube].neighborRemove++;
        counters[secondCube].neighborRemove++;
        LOG("Neighbor Remove: %d:%d - %d:%d\n",
            firstCube, firstSide, secondCube, secondSide);
        drawNeighbors(firstCube);
        drawNeighbors(secondCube);
    }

    void onNeighborAdd(unsigned firstCube, unsigned firstSide,
        unsigned secondCube, unsigned secondSide)
    {
        counters[firstCube].neighborAdd++;
        counters[secondCube].neighborAdd++;
        LOG("Neighbor Add: %d:%d - %d:%d\n",
            firstCube, firstSide, secondCube, secondSide);
        drawNeighbors(firstCube);
        drawNeighbors(secondCube);
    }

    void drawNeighbors(CubeID cube)
    {
        Neighborhood nb(cube);

        String<64> str;
        str << "nb "
            << Hex(nb.neighborAt(TOP), 2) << " "
            << Hex(nb.neighborAt(LEFT), 2) << " "
            << Hex(nb.neighborAt(BOTTOM), 2) << " "
            << Hex(nb.neighborAt(RIGHT), 2) << "\n";

        str << "   +" << counters[cube].neighborAdd
            << ", -" << counters[cube].neighborRemove
            << "\n\n";

        BG0ROMDrawable &draw = vid[cube].bg0rom;
        draw.text(vec(1,6), str);

        drawSideIndicator(draw, nb, vec( 1,  0), vec(14,  1), TOP);
        drawSideIndicator(draw, nb, vec( 0,  1), vec( 1, 14), LEFT);
        drawSideIndicator(draw, nb, vec( 1, 15), vec(14,  1), BOTTOM);
        drawSideIndicator(draw, nb, vec(15,  1), vec( 1, 14), RIGHT);
    }

    static void drawSideIndicator(BG0ROMDrawable &draw, Neighborhood &nb,
        Int2 topLeft, Int2 size, Side s)
    {
        unsigned nbColor = draw.ORANGE;
        draw.fill(topLeft, size,
            nbColor | (nb.hasNeighborAt(s) ? draw.SOLID_FG : draw.SOLID_BG));
    }
};


void main()
{
    static SensorListener sensors;

    sensors.install();

    // We're entirely event-driven. Everything is
    // updated by SensorListener's event callbacks.
    while (1)
        System::paint();
}
