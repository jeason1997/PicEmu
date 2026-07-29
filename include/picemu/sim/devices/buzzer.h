#ifndef SIM_DEVICES_BUZZER_H
#define SIM_DEVICES_BUZZER_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/* 蜂鸣器模型记录方波边沿并估算频率，不包含任何 SDL 音频代码。 */
typedef struct {
    SimDevice base;
    bool active;
    uint64_t transitions;
    uint64_t cycles_since_edge;
    uint64_t half_period_cycles;
    double frequency_hz;
} SimBuzzer;

void sim_buzzer_init(SimBuzzer *buzzer, const char *name);

#endif
