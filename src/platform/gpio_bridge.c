#include "picemu/platform/gpio_bridge.h"

#include <string.h>

void pic_hardware_bridge_init(PicHardwareBridge *bridge,
                              Pic10F200 *cpu,
                              const PicPlatformOps *ops)
{
    memset(bridge, 0, sizeof(*bridge));
    bridge->cpu = cpu;
    bridge->ops = ops;
    bridge->last_tris = 0xFFu;
}

void pic_hardware_bridge_sync(PicHardwareBridge *bridge)
{
    Pic10F200 *cpu;
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

        if (!bridge->initialized ||
            ((bridge->last_tris >> pin) & 1u) != !output) {
            if (bridge->ops->set_pin_mode != NULL) {
                bridge->ops->set_pin_mode(
                    bridge->ops->context, pin,
                    output ? PIC_PLATFORM_PIN_OUTPUT :
                             PIC_PLATFORM_PIN_INPUT);
            }
        }

        if (output) {
            if (bridge->ops->write_pin != NULL) {
                bridge->ops->write_pin(
                    bridge->ops->context, pin,
                    (cpu->gpio_latch & (1u << pin)) != 0);
            }
        } else if (bridge->ops->read_pin != NULL) {
            bool high = bridge->ops->read_pin(bridge->ops->context, pin);
            pic10f200_drive_pin(cpu, pin, true, high);
        }
    }

    bridge->last_tris = cpu->tris_gpio;
    bridge->initialized = true;
}
