#include "picemu/sim/device.h"

#include <string.h>

static void led_pin_changed(SimDevice *device, unsigned pin, SimLevel level)
{
    SimLed *led = device->state;
    if (pin == 0) {
        led->lit = led->active_high
            ? level == SIM_LEVEL_HIGH : level == SIM_LEVEL_LOW;
    }
}

static const SimDeviceOps LED_OPS = {
    .pin_changed = led_pin_changed
};

void sim_led_init(SimLed *led, const char *name,
                  uint8_t red, uint8_t green, uint8_t blue,
                  bool active_high)
{
    memset(led, 0, sizeof(*led));
    led->red = red;
    led->green = green;
    led->blue = blue;
    led->active_high = active_high;
    sim_device_init(&led->base, name, &LED_OPS, led, 1);
}
