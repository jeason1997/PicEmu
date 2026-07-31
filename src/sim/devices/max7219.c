#include "picemu/sim/devices/max7219.h"

#include <string.h>

static void update_outputs(SimMax7219 *chip)
{
    unsigned pin;
    uint8_t segments = sim_max7219_visible_row(chip, chip->scan_digit);
    for (pin = 0; pin < 8; ++pin) {
        sim_device_set_drive(&chip->base, SIM_MAX7219_SEG0 + pin,
            (segments & (1u << pin)) ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW);
        /* MAX7219 的位选输出 DIG0～DIG7 为低电平有效。 */
        sim_device_set_drive(&chip->base, SIM_MAX7219_DIG0 + pin,
            pin == chip->scan_digit && !chip->shutdown &&
            chip->scan_digit <= chip->scan_limit
            ? SIM_LEVEL_LOW : SIM_LEVEL_HIGH);
    }
}

static void write_register(SimMax7219 *chip, uint8_t address, uint8_t data)
{
    if (address >= 1 && address <= 8) chip->digits[address - 1] = data;
    else if (address == 0x09) chip->decode_mode = data;
    else if (address == 0x0A) chip->intensity = data & 0x0Fu;
    else if (address == 0x0B) chip->scan_limit = data & 0x07u;
    else if (address == 0x0C) chip->shutdown = (data & 1u) == 0;
    else if (address == 0x0F) chip->display_test = (data & 1u) != 0;
    update_outputs(chip);
}

static void max7219_reset(SimDevice *device)
{
    SimMax7219 *chip = device->state;
    memset(chip->digits, 0, sizeof(chip->digits));
    chip->shift_register = 0;
    chip->bit_count = 0;
    chip->clock = false;
    chip->load = true;
    chip->decode_mode = 0;
    chip->intensity = 0;
    chip->scan_limit = 7;
    chip->shutdown = true;
    chip->display_test = false;
    chip->scan_digit = 0;
    chip->scan_cycles = 0;
    update_outputs(chip);
}

static void max7219_tick(SimDevice *device, uint64_t cycles,
                         uint32_t cycles_per_second)
{
    SimMax7219 *chip = device->state;
    uint64_t slot = cycles_per_second / 8000u;
    if (slot == 0) slot = 1;
    chip->scan_cycles += cycles;
    while (chip->scan_cycles >= slot) {
        chip->scan_cycles -= slot;
        chip->scan_digit = (uint8_t)((chip->scan_digit + 1u) & 7u);
    }
    update_outputs(chip);
}

static void max7219_pin_changed(SimDevice *device, unsigned pin,
                                SimLevel level)
{
    SimMax7219 *chip = device->state;
    bool high = level == SIM_LEVEL_HIGH;
    if (pin == SIM_MAX7219_CLK) {
        if (!chip->load && high && !chip->clock) {
            chip->shift_register = (uint16_t)((chip->shift_register << 1) |
                (device->observed[SIM_MAX7219_DIN] == SIM_LEVEL_HIGH));
            if (chip->bit_count < 16) ++chip->bit_count;
        }
        chip->clock = high;
    } else if (pin == SIM_MAX7219_LOAD) {
        if (high && !chip->load && chip->bit_count == 16) {
            write_register(chip, (uint8_t)(chip->shift_register >> 8),
                           (uint8_t)chip->shift_register);
        }
        if (!high && chip->load) {
            chip->shift_register = 0;
            chip->bit_count = 0;
        }
        chip->load = high;
    }
}

static const SimDeviceOps MAX7219_OPS = {
    max7219_reset, max7219_tick, max7219_pin_changed
};

void sim_max7219_init(SimMax7219 *chip, const char *name)
{
    memset(chip, 0, sizeof(*chip));
    sim_device_init(&chip->base, name, &MAX7219_OPS, chip,
                    SIM_MAX7219_PIN_COUNT);
    max7219_reset(&chip->base);
}

uint8_t sim_max7219_visible_row(const SimMax7219 *chip, unsigned row)
{
    if (chip == NULL || row >= 8 || chip->shutdown || row > chip->scan_limit)
        return 0;
    return chip->display_test ? 0xFFu : chip->digits[row];
}
