#ifndef SIM_MCU_H
#define SIM_MCU_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct SimMcu SimMcu;

typedef struct {
    void (*reset)(SimMcu *mcu);
    unsigned (*step)(SimMcu *mcu);
    unsigned (*pin_count)(const SimMcu *mcu);
    SimLevel (*pin_drive)(const SimMcu *mcu, unsigned pin);
    void (*set_pin_input)(SimMcu *mcu, unsigned pin, SimLevel level);
    bool (*stopped)(const SimMcu *mcu);
} SimMcuOps;

/*
 * 电路层只依赖这个抽象接口。state 指向具体 CPU，cycles_per_second
 * 表示 step() 返回周期所使用的时基。
 */
struct SimMcu {
    const SimMcuOps *ops;
    void *state;
    uint32_t cycles_per_second;
};

void sim_mcu_reset(SimMcu *mcu);
unsigned sim_mcu_step(SimMcu *mcu);
unsigned sim_mcu_pin_count(const SimMcu *mcu);
SimLevel sim_mcu_pin_drive(const SimMcu *mcu, unsigned pin);
void sim_mcu_set_pin_input(SimMcu *mcu, unsigned pin, SimLevel level);
bool sim_mcu_stopped(const SimMcu *mcu);

#endif
