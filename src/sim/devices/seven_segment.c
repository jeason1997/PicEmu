#include "picemu/sim/devices/seven_segment.h"

#include <string.h>

static void seven_segment_reset(SimDevice *device)
{
    SimSevenSegment *display = device->state;
    display->segments = 0;
}

static void seven_segment_pin_changed(SimDevice *device, unsigned pin,
                                      SimLevel level)
{
    SimSevenSegment *display = device->state;
    uint8_t mask;
    if (pin >= 8u) return;
    mask = (uint8_t)(1u << pin);
    if (level == SIM_LEVEL_HIGH) display->segments |= mask;
    else display->segments &= (uint8_t)~mask;
}

static const SimDeviceOps SEVEN_SEGMENT_OPS = {
    .reset = seven_segment_reset,
    .tick = NULL,
    .pin_changed = seven_segment_pin_changed
};

void sim_seven_segment_init(SimSevenSegment *display, const char *name,
                            bool active_high)
{
    memset(display, 0, sizeof(*display));
    display->active_high = active_high;
    sim_device_init(&display->base, name, &SEVEN_SEGMENT_OPS, display, 8);
}

uint8_t sim_seven_segment_visible_segments(const SimSevenSegment *display)
{
    return display->active_high
        ? display->segments : (uint8_t)~display->segments;
}
