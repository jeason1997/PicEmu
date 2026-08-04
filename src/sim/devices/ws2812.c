#include "picemu/sim/devices/ws2812.h"

#include <string.h>

/*
 * 新版 WS2812B 常用 280 us 复位间隔。较长阈值也避免把低主频教学固件在
 * 两颗像素之间的颜色计算误判为一帧结束；固件显式保持 300 us 才锁存。
 */
static uint64_t latch_cycles(const SimWs2812 *strip)
{
    return ((uint64_t)strip->cycles_per_second * 280u + 999999u) / 1000000u;
}

static void latch(SimWs2812 *strip)
{
    unsigned led;
    unsigned complete_leds = strip->bit_count / 24u;

    /*
     * WS2812 不知道整条链有多长：复位到来时，收到完整 24 位的前若干颗灯
     * 立即更新，链尾没有收到新数据的灯保持原颜色。末尾不足 24 位的残帧
     * 不属于任何完整像素，因此直接忽略。
     */
    for (led = 0; led < complete_leds; ++led) {
        /* 线上字节顺序是 GRB，模型公开状态则保持更直观的 RGB。 */
        strip->colors[led][0] = strip->pending[led * 3u + 1u];
        strip->colors[led][1] = strip->pending[led * 3u];
        strip->colors[led][2] = strip->pending[led * 3u + 2u];
    }
}

static void reset(SimDevice *device)
{
    SimWs2812 *strip = device->state;
    memset(strip->colors, 0, sizeof(strip->colors));
    memset(strip->pending, 0, sizeof(strip->pending));
    strip->bit_count = 0;
    strip->level_cycles = 0;
    strip->cycles_per_second = 0;
    strip->high = false;
}

static void tick(SimDevice *device, uint64_t cycles,
                 uint32_t cycles_per_second)
{
    SimWs2812 *strip = device->state;
    strip->cycles_per_second = cycles_per_second;
    strip->level_cycles += cycles;
    if (!strip->high && strip->bit_count != 0 &&
        strip->level_cycles >= latch_cycles(strip)) {
        latch(strip);
        strip->bit_count = 0;
    }
}

static void pin_changed(SimDevice *device, unsigned pin, SimLevel level)
{
    SimWs2812 *strip = device->state;
    bool high = level == SIM_LEVEL_HIGH;
    (void)pin;
    if (strip->high && !high && strip->bit_count < strip->led_count * 24u) {
        unsigned byte = strip->bit_count / 8u;
        unsigned bit = 7u - strip->bit_count % 8u;
        /* 标准器件按约 0.6 us 分界；4 MHz 教学固件以 1/2 指令周期编码。 */
        uint64_t threshold = strip->cycles_per_second >= 2000000u
            ? ((uint64_t)strip->cycles_per_second * 600u + 999999999u) /
              1000000000u
            : 2u;
        if (strip->level_cycles >= threshold) strip->pending[byte] |= 1u << bit;
        else strip->pending[byte] &= (uint8_t)~(1u << bit);
        ++strip->bit_count;
    }
    strip->high = high;
    strip->level_cycles = 0;
}

static const SimDeviceOps OPS = {reset, tick, pin_changed};

void sim_ws2812_init(SimWs2812 *strip, const char *name,
                     unsigned led_count)
{
    memset(strip, 0, sizeof(*strip));
    strip->led_count = led_count > SIM_WS2812_MAX_LEDS
        ? SIM_WS2812_MAX_LEDS : led_count;
    if (strip->led_count == 0) strip->led_count = 1;
    strip->rows = 1;
    strip->cols = strip->led_count;
    sim_device_init(&strip->base, name, &OPS, strip, 1);
}

void sim_ws2812_set_input(SimWs2812 *strip, bool high)
{
    SimLevel level = high ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
    if (strip->base.observed[0] == level) return;
    strip->base.observed[0] = level;
    pin_changed(&strip->base, 0, level);
}
