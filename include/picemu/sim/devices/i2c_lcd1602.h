#ifndef PICEMU_SIM_DEVICES_I2C_LCD1602_H
#define PICEMU_SIM_DEVICES_I2C_LCD1602_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t address;
    bool scl;
    bool sda;
    bool active;
    bool addressed;
    uint8_t input;
    unsigned bits;
    bool ack_clock;
    uint8_t port;
    bool last_enable;
    bool have_high_nibble;
    uint8_t high_nibble;
    uint8_t ddram[80];
    uint8_t cursor;
    bool display_on;
    bool increment;
} SimI2cLcd1602;

void sim_i2c_lcd1602_init(SimI2cLcd1602 *lcd, uint8_t address);
void sim_i2c_lcd1602_reset(SimI2cLcd1602 *lcd);
void sim_i2c_lcd1602_set_lines(SimI2cLcd1602 *lcd, bool scl, bool sda);
void sim_i2c_lcd1602_get_line(const SimI2cLcd1602 *lcd, unsigned line,
                              char output[17]);

#endif
