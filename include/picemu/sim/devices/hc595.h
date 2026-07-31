#ifndef PICEMU_SIM_DEVICES_HC595_H
#define PICEMU_SIM_DEVICES_HC595_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * 74HC595 八位串入并出移位寄存器。
 *
 * 输入引脚依次为 SER、SRCLK、RCLK。SRCLK 上升沿把 SER 移入内部寄存器，
 * RCLK 上升沿把移位寄存器复制到并行输出 Q0～Q7。输出值独立于具体负载，
 * 因而同一模型可供七段数码管、LED 点阵或普通 LED 阵列复用。
 */
typedef struct {
    SimDevice base;
    uint8_t shift_register;
    uint8_t outputs;
    bool data;
    bool clock;
    bool latch;
} SimHc595;

void sim_hc595_init(SimHc595 *chip, const char *name);

#endif
