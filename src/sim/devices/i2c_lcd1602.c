#include "picemu/sim/devices/i2c_lcd1602.h"

#include <string.h>

static unsigned ddram_index(uint8_t address)
{
    if (address >= 0x40u && address < 0x68u) return 40u + address - 0x40u;
    return address < 40u ? address : 79u;
}

static void lcd_command(SimI2cLcd1602 *lcd, uint8_t value)
{
    if ((value & 0x80u) != 0) {
        lcd->cursor = value & 0x7Fu;
    } else if (value == 0x01u) {
        memset(lcd->ddram, ' ', sizeof(lcd->ddram));
        lcd->cursor = 0;
    } else if (value == 0x02u) {
        lcd->cursor = 0;
    } else if ((value & 0xFCu) == 0x04u) {
        lcd->increment = (value & 0x02u) != 0;
    } else if ((value & 0xF8u) == 0x08u) {
        lcd->display_on = (value & 0x04u) != 0;
    }
}

static void lcd_byte(SimI2cLcd1602 *lcd, uint8_t value, bool data)
{
    if (data) {
        lcd->ddram[ddram_index(lcd->cursor)] = value;
        lcd->cursor = (uint8_t)(lcd->cursor + (lcd->increment ? 1u : -1u));
    } else {
        lcd_command(lcd, value);
    }
}

static void port_write(SimI2cLcd1602 *lcd, uint8_t value)
{
    bool enable = (value & 0x04u) != 0;
    /* 常见PCF8574转接板映射：P0=RS、P2=E、P4至P7=D4至D7。 */
    if (lcd->last_enable && !enable) {
        uint8_t nibble = value & 0xF0u;
        if (!lcd->have_high_nibble) {
            lcd->high_nibble = nibble;
            lcd->have_high_nibble = true;
        } else {
            lcd_byte(lcd, (uint8_t)(lcd->high_nibble | (nibble >> 4)),
                     (value & 0x01u) != 0);
            lcd->have_high_nibble = false;
        }
    }
    lcd->last_enable = enable;
    lcd->port = value;
}

static void accept_byte(SimI2cLcd1602 *lcd, uint8_t value)
{
    if (!lcd->addressed) {
        lcd->addressed = (value >> 1) == lcd->address && (value & 1u) == 0;
    } else {
        port_write(lcd, value);
    }
}

void sim_i2c_lcd1602_reset(SimI2cLcd1602 *lcd)
{
    uint8_t address = lcd->address;
    memset(lcd, 0, sizeof(*lcd));
    lcd->address = address;
    lcd->scl = true;
    lcd->sda = true;
    lcd->display_on = true;
    lcd->increment = true;
    memset(lcd->ddram, ' ', sizeof(lcd->ddram));
}

void sim_i2c_lcd1602_init(SimI2cLcd1602 *lcd, uint8_t address)
{
    memset(lcd, 0, sizeof(*lcd));
    lcd->address = address;
    sim_i2c_lcd1602_reset(lcd);
}

void sim_i2c_lcd1602_set_lines(SimI2cLcd1602 *lcd, bool scl, bool sda)
{
    if (lcd->scl && scl) {
        if (lcd->sda && !sda) {
            lcd->active = true;
            lcd->addressed = false;
            lcd->input = 0;
            lcd->bits = 0;
            lcd->ack_clock = false;
        } else if (!lcd->sda && sda) {
            lcd->active = false;
        }
    }
    if (lcd->active && !lcd->scl && scl) {
        if (lcd->ack_clock) {
            lcd->ack_clock = false;
        } else {
            lcd->input = (uint8_t)((lcd->input << 1) | (sda ? 1u : 0u));
            if (++lcd->bits == 8u) {
                accept_byte(lcd, lcd->input);
                lcd->input = 0;
                lcd->bits = 0;
                lcd->ack_clock = true;
            }
        }
    }
    lcd->scl = scl;
    lcd->sda = sda;
}

void sim_i2c_lcd1602_get_line(const SimI2cLcd1602 *lcd, unsigned line,
                              char output[17])
{
    unsigned start = line == 0 ? 0u : 40u;
    unsigned i;
    for (i = 0; i < 16u; ++i) {
        uint8_t value = lcd->display_on ? lcd->ddram[start + i] : ' ';
        output[i] = value >= 0x20u && value <= 0x7Eu ? (char)value : '?';
    }
    output[16] = '\0';
}
