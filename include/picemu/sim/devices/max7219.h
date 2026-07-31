#ifndef PICEMU_SIM_DEVICES_MAX7219_H
#define PICEMU_SIM_DEVICES_MAX7219_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

/* 前三个引脚是串行输入，随后是八路 SEG 和八路低电平有效 DIG 输出。 */
enum {
    SIM_MAX7219_DIN,
    SIM_MAX7219_CLK,
    SIM_MAX7219_LOAD,
    SIM_MAX7219_SEG0,
    SIM_MAX7219_DIG0 = SIM_MAX7219_SEG0 + 8,
    SIM_MAX7219_PIN_COUNT = SIM_MAX7219_DIG0 + 8
};

typedef struct {
    SimDevice base;
    uint16_t shift_register;
    uint8_t bit_count;
    bool clock;
    bool load;
    uint8_t digits[8];
    uint8_t decode_mode;
    uint8_t intensity;
    uint8_t scan_limit;
    bool shutdown;
    bool display_test;
    uint8_t scan_digit;
    uint64_t scan_cycles;
} SimMax7219;

void sim_max7219_init(SimMax7219 *chip, const char *name);
uint8_t sim_max7219_visible_row(const SimMax7219 *chip, unsigned row);

#endif
