#ifndef SIM_DEVICES_LED_H
#define SIM_DEVICES_LED_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/* LED 的状态和接口只对真正使用 LED 的模块可见。 */
typedef struct {
    SimDevice base;
    bool lit;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool active_high;
} SimLed;

void sim_led_init(SimLed *led, const char *name,
                  uint8_t red, uint8_t green, uint8_t blue,
                  bool active_high);

#endif
