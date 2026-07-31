#include "picemu/sim/devices/led_matrix_8x8.h"

#include <string.h>

static void refresh(SimLedMatrix8x8 *matrix)
{
    unsigned row, column;
    for (row = 0; row < 8; ++row) {
        uint8_t value = 0;
        SimLevel common = matrix->base.observed[8 + row];
        bool selected = matrix->common_cathode
            ? common == SIM_LEVEL_LOW : common == SIM_LEVEL_HIGH;
        /* 未选中的行保持上次扫描值，模拟人眼看到的一帧完整图像。 */
        if (!selected) continue;
        for (column = 0; column < 8; ++column) {
            SimLevel segment = matrix->base.observed[column];
            bool lit = matrix->common_cathode
                ? segment == SIM_LEVEL_HIGH && common == SIM_LEVEL_LOW
                : segment == SIM_LEVEL_LOW && common == SIM_LEVEL_HIGH;
            if (lit) value |= (uint8_t)(1u << column);
        }
        matrix->rows[row] = value;
    }
}

static void matrix_reset(SimDevice *device)
{
    SimLedMatrix8x8 *matrix = device->state;
    memset(matrix->rows, 0, sizeof(matrix->rows));
}

static void matrix_pin_changed(SimDevice *device, unsigned pin,
                               SimLevel level)
{
    (void)pin;
    (void)level;
    refresh(device->state);
}

static const SimDeviceOps MATRIX_OPS = {
    matrix_reset, NULL, matrix_pin_changed
};

void sim_led_matrix_8x8_init(SimLedMatrix8x8 *matrix, const char *name,
                             bool common_cathode)
{
    memset(matrix, 0, sizeof(*matrix));
    matrix->common_cathode = common_cathode;
    sim_device_init(&matrix->base, name, &MATRIX_OPS, matrix, 16);
}

uint8_t sim_led_matrix_8x8_row(const SimLedMatrix8x8 *matrix, unsigned row)
{
    return matrix != NULL && row < 8 ? matrix->rows[row] : 0;
}
