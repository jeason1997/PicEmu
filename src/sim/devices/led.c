#include "picemu/sim/devices/led.h"

#include <string.h>

static void led_pin_changed(SimDevice *device, unsigned pin, SimLevel level)
{
    SimLed *led = device->state;
    if (pin == 0) {
        led->lit = led->active_high
            ? level == SIM_LEVEL_HIGH : level == SIM_LEVEL_LOW;
    }
}

static void led_reset(SimDevice *device)
{
    SimLed *led = device->state;
    led->lit = false;
    led->brightness = 0;
    led->sample_cycles = 0;
    led->lit_cycles = 0;
}

static void led_tick(SimDevice *device, uint64_t cycles,
                     uint32_t cycles_per_second)
{
    SimLed *led = device->state;
    uint64_t window = cycles_per_second / 60u;

    if (window == 0) window = 1;
    if (led->lit) led->lit_cycles += cycles;
    led->sample_cycles += cycles;

    /*
     * 每个显示帧附近更新一次亮度。窗口内高电平所占比例就是 PWM
     * 占空比；使用整数计算，核心仿真层不依赖浮点或前端实现。
     */
    if (led->sample_cycles >= window) {
        led->brightness = (uint8_t)(
            led->lit_cycles * 255u / led->sample_cycles);
        led->sample_cycles = 0;
        led->lit_cycles = 0;
    }
}

static const SimDeviceOps LED_OPS = {
    .reset = led_reset,
    .tick = led_tick,
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
