#ifndef PICEMU_SIM_DEVICES_SEVEN_SEGMENT_H
#define PICEMU_SIM_DEVICES_SEVEN_SEGMENT_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/* 裸七段数码管；八个输入引脚依次对应 a、b、c、d、e、f、g、dp。 */
typedef struct {
    SimDevice base;
    uint8_t segments;
    bool active_high;
} SimSevenSegment;

void sim_seven_segment_init(SimSevenSegment *display, const char *name,
                            bool active_high);
uint8_t sim_seven_segment_visible_segments(const SimSevenSegment *display);

#endif
