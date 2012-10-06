#include "homebutton.h"
#include "tasks.h"
#include "macros.h"
#include "pause.h"

namespace HomeButton
{

static uint32_t state;


void init()
{
    state = 0;
}

void setPressed(bool value)
{
    if (state != value) {
        state = value;
        HomeButton::update();
        Pause::taskWork.atomicMark(Pause::ButtonPress);
        Tasks::trigger(Tasks::Pause);
    }
}

bool isPressed()
{
    return state;
}

} // namespace HomeButton
