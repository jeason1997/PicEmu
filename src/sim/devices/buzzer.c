#include "picemu/sim/device.h"

#include <string.h>

static void buzzer_pin_changed(SimDevice *device,
                               unsigned pin, SimLevel level)
{
    SimBuzzer *buzzer = device->state;
    bool active = level == SIM_LEVEL_HIGH;
    if (pin == 0 && active != buzzer->active) {
        buzzer->active = active;
        ++buzzer->transitions;
    }
}

static const SimDeviceOps BUZZER_OPS = {
    .pin_changed = buzzer_pin_changed
};

void sim_buzzer_init(SimBuzzer *buzzer, const char *name)
{
    memset(buzzer, 0, sizeof(*buzzer));
    sim_device_init(&buzzer->base, name, &BUZZER_OPS, buzzer, 1);
}
