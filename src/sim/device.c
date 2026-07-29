#include "picemu/sim/device.h"

#include <string.h>

void sim_device_init(SimDevice *device, const char *name,
                     const SimDeviceOps *ops, void *state,
                     unsigned pin_count)
{
    unsigned pin;

    memset(device, 0, sizeof(*device));
    device->name = name;
    device->ops = ops;
    device->state = state;
    device->pin_count = pin_count <= SIM_DEVICE_MAX_PINS
        ? pin_count : SIM_DEVICE_MAX_PINS;
    for (pin = 0; pin < SIM_DEVICE_MAX_PINS; ++pin) {
        device->drive[pin] = SIM_LEVEL_Z;
        device->observed[pin] = SIM_LEVEL_Z;
    }
}

void sim_device_set_drive(SimDevice *device, unsigned pin, SimLevel level)
{
    if (device != NULL && pin < device->pin_count) {
        device->drive[pin] = level;
    }
}
