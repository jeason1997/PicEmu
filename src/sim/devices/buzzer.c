#include "picemu/sim/devices/buzzer.h"

#include <string.h>

enum {
    /* 当前模拟器按1MHz PIC指令周期运行。 */
    PIC_CYCLES_PER_SECOND = 1000000,
    BUZZER_SIGNAL_TIMEOUT_CYCLES = 50000
};

static void buzzer_reset(SimDevice *device)
{
    SimBuzzer *buzzer = device->state;

    buzzer->active = false;
    buzzer->transitions = 0;
    buzzer->cycles_since_edge = 0;
    buzzer->half_period_cycles = 0;
    buzzer->frequency_hz = 0.0;
}

static void buzzer_tick(SimDevice *device, uint64_t cycles)
{
    SimBuzzer *buzzer = device->state;

    buzzer->cycles_since_edge += cycles;
    /*
     * 长时间没有边沿表示方波已经停止。固定高电平仍保留active状态，
     * SDL可将它当成传统有源蜂鸣器处理。
     */
    if (buzzer->cycles_since_edge > BUZZER_SIGNAL_TIMEOUT_CYCLES) {
        buzzer->half_period_cycles = 0;
        buzzer->frequency_hz = 0.0;
    }
}

static void buzzer_pin_changed(SimDevice *device,
                               unsigned pin, SimLevel level)
{
    SimBuzzer *buzzer = device->state;
    bool active = level == SIM_LEVEL_HIGH;

    if (pin == 0 && active != buzzer->active) {
        if (buzzer->transitions > 0 && buzzer->cycles_since_edge > 0 &&
            buzzer->cycles_since_edge <= BUZZER_SIGNAL_TIMEOUT_CYCLES) {
            buzzer->half_period_cycles = buzzer->cycles_since_edge;
            buzzer->frequency_hz =
                (double)PIC_CYCLES_PER_SECOND /
                (2.0 * (double)buzzer->half_period_cycles);
        }
        buzzer->cycles_since_edge = 0;
        buzzer->active = active;
        ++buzzer->transitions;
    }
}

static const SimDeviceOps BUZZER_OPS = {
    .reset = buzzer_reset,
    .tick = buzzer_tick,
    .pin_changed = buzzer_pin_changed
};

void sim_buzzer_init(SimBuzzer *buzzer, const char *name)
{
    memset(buzzer, 0, sizeof(*buzzer));
    sim_device_init(&buzzer->base, name, &BUZZER_OPS, buzzer, 1);
}
