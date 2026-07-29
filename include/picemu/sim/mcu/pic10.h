#ifndef SIM_MCU_PIC10_H
#define SIM_MCU_PIC10_H

#include "picemu/core/pic10_cpu.h"
#include "picemu/sim/mcu.h"

typedef struct {
    SimMcu base;
    Pic10Cpu *cpu;
} SimPic10Mcu;

void sim_pic10_mcu_init(SimPic10Mcu *adapter, Pic10Cpu *cpu,
                        uint32_t oscillator_hz);

#endif
