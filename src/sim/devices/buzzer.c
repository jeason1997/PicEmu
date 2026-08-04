#include "picemu/sim/devices/buzzer.h"

#include <string.h>

enum { BUZZER_SIGNAL_TIMEOUT_MS = 50 };

static void buzzer_reset(SimDevice *device)
{
    SimBuzzer *buzzer = device->state;

    buzzer->active = false;
    buzzer->transitions = 0;
    buzzer->cycles_since_edge = 0;
    buzzer->half_period_cycles = 0;
    buzzer->frequency_hz = 0.0;
}

static void buzzer_tick(SimDevice *device, uint64_t cycles,
                        uint32_t cycles_per_second)
{
    SimBuzzer *buzzer = device->state;

    buzzer->cycles_per_second = cycles_per_second;
    buzzer->cycles_since_edge += cycles;
    /*
     * 长时间没有边沿表示方波已经停止。固定高电平仍保留active状态，
     * 前端可将它当成传统有源蜂鸣器处理。
     */
    if (buzzer->cycles_since_edge >
        (uint64_t)cycles_per_second * BUZZER_SIGNAL_TIMEOUT_MS / 1000u) {
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
        uint64_t timeout =
            (uint64_t)buzzer->cycles_per_second *
            BUZZER_SIGNAL_TIMEOUT_MS / 1000u;
        if (buzzer->transitions > 0 && buzzer->cycles_since_edge > 0 &&
            buzzer->cycles_since_edge <= timeout &&
            buzzer->cycles_per_second > 0) {
            buzzer->half_period_cycles = buzzer->cycles_since_edge;
            buzzer->frequency_hz =
                (double)buzzer->cycles_per_second /
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
