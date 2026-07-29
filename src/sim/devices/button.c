#include "picemu/sim/device.h"

#include <string.h>

static void button_update_drive(SimButton *button)
{
    bool high = button->pressed ? !button->active_low : button->active_low;
    sim_device_set_drive(&button->base, 0,
                         high ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW);
}

static void button_reset(SimDevice *device)
{
    SimButton *button = device->state;
    button->pressed = false;
    button_update_drive(button);
}

static const SimDeviceOps BUTTON_OPS = {
    .reset = button_reset
};

void sim_button_init(SimButton *button, const char *name, bool active_low)
{
    memset(button, 0, sizeof(*button));
    button->active_low = active_low;
    sim_device_init(&button->base, name, &BUTTON_OPS, button, 1);
    button_update_drive(button);
}

void sim_button_set_pressed(SimButton *button, bool pressed)
{
    button->pressed = pressed;
    button_update_drive(button);
}
