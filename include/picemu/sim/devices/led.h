#ifndef SIM_DEVICES_LED_H
#define SIM_DEVICES_LED_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/* LED 的状态和接口只对真正使用 LED 的模块可见。 */
typedef struct {
    SimDevice base;
    bool lit;
    /* 0~255 的平均亮度。高速软件 PWM 会按占空比显示，而不是闪烁。 */
    uint8_t brightness;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool active_high;
    uint64_t sample_cycles;
    uint64_t lit_cycles;
} SimLed;

void sim_led_init(SimLed *led, const char *name,
                  uint8_t red, uint8_t green, uint8_t blue,
                  bool active_high);

#endif
