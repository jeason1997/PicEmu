#ifndef PICEMU_SIM_DEVICES_LED_MATRIX_8X8_H
#define PICEMU_SIM_DEVICES_LED_MATRIX_8X8_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    SimDevice base;
    bool common_cathode;
    uint8_t rows[8];
} SimLedMatrix8x8;

void sim_led_matrix_8x8_init(SimLedMatrix8x8 *matrix, const char *name,
                             bool common_cathode);
uint8_t sim_led_matrix_8x8_row(const SimLedMatrix8x8 *matrix, unsigned row);

#endif
