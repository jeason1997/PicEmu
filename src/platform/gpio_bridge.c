#include "picemu/platform/gpio_bridge.h"

#include <string.h>

void pic_hardware_bridge_init(PicHardwareBridge *bridge,
                              Pic10Cpu *cpu,
                              const PicPlatformOps *ops)
{
    memset(bridge, 0, sizeof(*bridge));
    bridge->cpu = cpu;
    bridge->ops = ops;
    bridge->last_tris = 0xFFu;
}

void pic_hardware_bridge_sync(PicHardwareBridge *bridge)
{
    Pic10Cpu *cpu;
    unsigned pin;

    if (bridge == NULL || bridge->cpu == NULL || bridge->ops == NULL) {
        return;
    }
    cpu = bridge->cpu;

    for (pin = 0; pin < cpu->device->gpio_count; ++pin) {
        bool output_capable =
            (cpu->device->pins[pin].capabilities & PIC_PIN_CAP_OUTPUT) != 0;
        bool output = output_capable &&
                      (cpu->tris_gpio & (1u << pin)) == 0;
        bool mode_changed =
            !bridge->initialized ||
            ((bridge->last_tris >> pin) & 1u) != !output;

        if (mode_changed) {
            if (bridge->ops->set_pin_mode != NULL) {
                bridge->ops->set_pin_mode(
                    bridge->ops->context, pin,
                    output ? PIC_PLATFORM_PIN_OUTPUT :
                             PIC_PLATFORM_PIN_INPUT);
            }
        }

        if (output) {
            bool high = (cpu->gpio_latch & (1u << pin)) != 0;
            bool level_changed =
                !bridge->initialized ||
                ((bridge->last_output >> pin) & 1u) != high;

            if ((mode_changed || level_changed) &&
                bridge->ops->write_pin != NULL) {
                bridge->ops->write_pin(
                    bridge->ops->context, pin, high);
            }
        } else if (bridge->ops->read_pin != NULL) {
            bool high = bridge->ops->read_pin(bridge->ops->context, pin);
            pic10_drive_pin(cpu, pin, true, high);
        }
    }

    bridge->last_tris = cpu->tris_gpio;
    bridge->last_output = cpu->gpio_latch;
    bridge->initialized = true;
}
