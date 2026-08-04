#ifndef SIM_WS2812_H
#define SIM_WS2812_H

#include "picemu/sim/device.h"

#include <stdbool.h>
#include <stdint.h>

#define SIM_WS2812_MAX_LEDS 1024u

typedef struct {
    SimDevice base;
    unsigned led_count;
    unsigned rows;
    unsigned cols;
    bool serpentine;
    uint8_t colors[SIM_WS2812_MAX_LEDS][3]; /* 对外统一使用 RGB 顺序。 */
    uint8_t pending[SIM_WS2812_MAX_LEDS * 3u];
    unsigned bit_count;
    uint64_t level_cycles;
    uint32_t cycles_per_second;
    bool high;
} SimWs2812;

void sim_ws2812_init(SimWs2812 *strip, const char *name,
                     unsigned led_count);
void sim_ws2812_set_input(SimWs2812 *strip, bool high);

#endif
