#include "picemu/sim/mcu/pic10.h"

static void pic10_mcu_reset(SimMcu *mcu)
{
    SimPic10Mcu *adapter = mcu->state;
    pic10_reset(adapter->cpu, PIC10_RESET_MCLR);
}

static unsigned pic10_mcu_step(SimMcu *mcu)
{
    SimPic10Mcu *adapter = mcu->state;
    return pic10_step_cycles(adapter->cpu);
}

static unsigned pic10_mcu_pin_count(const SimMcu *mcu)
{
    const SimPic10Mcu *adapter = mcu->state;
    return adapter->cpu->device->gpio_count;
}

static SimLevel pic10_mcu_pin_drive(const SimMcu *mcu, unsigned pin)
{
    const SimPic10Mcu *adapter = mcu->state;
    const Pic10Cpu *cpu = adapter->cpu;

    if (pin >= cpu->device->gpio_count ||
        (cpu->device->pins[pin].capabilities & PIC_PIN_CAP_OUTPUT) == 0 ||
        (cpu->tris_gpio & (1u << pin)) != 0) {
        return SIM_LEVEL_Z;
    }
    return (cpu->gpio_latch & (1u << pin)) != 0
        ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
}

static void pic10_mcu_set_pin_input(SimMcu *mcu, unsigned pin,
                                    SimLevel level)
{
    SimPic10Mcu *adapter = mcu->state;
    bool driven = level == SIM_LEVEL_LOW || level == SIM_LEVEL_HIGH;
    pic10_drive_pin(adapter->cpu, pin, driven, level == SIM_LEVEL_HIGH);
}

static bool pic10_mcu_stopped(const SimMcu *mcu)
{
    const SimPic10Mcu *adapter = mcu->state;
    return adapter->cpu->stopped;
}

static const SimMcuOps PIC10_MCU_OPS = {
    .reset = pic10_mcu_reset,
    .step = pic10_mcu_step,
    .pin_count = pic10_mcu_pin_count,
    .pin_drive = pic10_mcu_pin_drive,
    .set_pin_input = pic10_mcu_set_pin_input,
    .stopped = pic10_mcu_stopped
};

void sim_pic10_mcu_init(SimPic10Mcu *adapter, Pic10Cpu *cpu,
                        uint32_t oscillator_hz)
{
    adapter->cpu = cpu;
    adapter->base.ops = &PIC10_MCU_OPS;
    adapter->base.state = adapter;
    /* Baseline PIC 的一个指令周期等于四个振荡器周期。 */
    adapter->base.cycles_per_second =
        oscillator_hz >= 4u ? oscillator_hz / 4u : 1u;
}
