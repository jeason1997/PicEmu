#ifndef SIM_DEVICES_BUTTON_H
#define SIM_DEVICES_BUTTON_H

#include "picemu/sim/device.h"

#include <stdbool.h>

/* 按键模型可以由 SDL 鼠标或嵌入式平台的真实 GPIO 驱动。 */
typedef struct {
    SimDevice base;
    bool pressed;
    bool active_low;
} SimButton;

void sim_button_init(SimButton *button, const char *name, bool active_low);
void sim_button_set_pressed(SimButton *button, bool pressed);

#endif
