#include <sifteo.h>
using namespace Sifteo;

Cube c;

void siftmain()
{
    c.enable(0);
    VidMode_BG0_ROM vid(c.vbuf);
    vid.init();
    vid.BG0_text(Vec2(1,1), "Hello World");
    while (1)
        System::paint();
}
