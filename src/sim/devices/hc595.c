#include "picemu/sim/devices/hc595.h"

#include <string.h>

enum {
    HC595_DATA_PIN = 0,
    HC595_CLOCK_PIN = 1,
    HC595_LATCH_PIN = 2
};

static void hc595_reset(SimDevice *device)
{
    SimHc595 *chip = device->state;
    chip->shift_register = 0;
    chip->outputs = 0;
    chip->data = false;
    chip->clock = false;
    chip->latch = false;
}

static void hc595_pin_changed(SimDevice *device, unsigned pin, SimLevel level)
{
    SimHc595 *chip = device->state;
    bool high = level == SIM_LEVEL_HIGH;

    if (pin == HC595_DATA_PIN) {
        chip->data = high;
    } else if (pin == HC595_CLOCK_PIN) {
        if (high && !chip->clock) {
            chip->shift_register = (uint8_t)(
                (chip->shift_register << 1) | (chip->data ? 1u : 0u));
        }
        chip->clock = high;
    } else if (pin == HC595_LATCH_PIN) {
        if (high && !chip->latch) chip->outputs = chip->shift_register;
        chip->latch = high;
    }
}

static const SimDeviceOps HC595_OPS = {
    .reset = hc595_reset,
    .tick = NULL,
    .pin_changed = hc595_pin_changed
};

void sim_hc595_init(SimHc595 *chip, const char *name)
{
    memset(chip, 0, sizeof(*chip));
    sim_device_init(&chip->base, name, &HC595_OPS, chip, 3);
}
