#include "board_config.h"
#include "firmware.h"
#include "picemu/core/pic10_cpu.h"
#include "picemu/platform/gpio_bridge.h"
#include "py32f0xx_ll_rcc.h"
#include "py32f0xx_ll_utils.h"

#include <stdbool.h>
#include <stdint.h>

#define SYSTEM_CLOCK_HZ          48000000u
#define RCC_PLLCFGR_REGISTER     (*(volatile uint32_t *)(RCC_BASE + 0x0cu))
#define RCC_CR_PLLON_HIDDEN      (1ul << 24)
#define RCC_CR_PLLRDY_HIDDEN     (1ul << 25)
#define RCC_CFGR_SW_PLL_HIDDEN   (2ul << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SWS_PLL_HIDDEN  (2ul << RCC_CFGR_SWS_Pos)
#define SYSTICK_MASK             0x00ffffffu

#ifndef PY32_PIC10F202
#define PY32_PIC10F202 0
#endif

enum {
    PIC_OSCILLATOR_HZ = 4000000u,
    PIC_CYCLES_PER_SECOND = PIC_OSCILLATOR_HZ / 4u,
    PY32_TICKS_PER_PIC_CYCLE = SYSTEM_CLOCK_HZ / PIC_CYCLES_PER_SECOND,
    PIC_GPIO_COUNT = 4u,
    GPIO_INPUT_POLL_CYCLES = 64u,
    SCHEDULER_MAX_LAG_TICKS = SYSTEM_CLOCK_HZ / 100u
};

static Pic10Cpu pic_cpu;
static PicHardwareBridge gpio_bridge;
static uint32_t timer_last_value;
static uint32_t timer_elapsed_ticks;

static void system_clock_config(void)
{
    /* 参考 Blink 工程：24 MHz HSI 作为固定二倍频 PLL 的输入。 */
    LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_24MHz);
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1u) {
    }

    /* 48 MHz 时 Flash 必须使用一个等待周期，否则取指可能不稳定。 */
    if (LL_SetFlashLatency(SYSTEM_CLOCK_HZ) != SUCCESS) {
        for (;;) {
        }
    }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    /* PY32F002A 设备头隐藏了 PLL 位，寄存器布局与参考工程保持一致。 */
    RCC_PLLCFGR_REGISTER = 0u;
    SET_BIT(RCC->CR, RCC_CR_PLLON_HIDDEN);
    while ((RCC->CR & RCC_CR_PLLRDY_HIDDEN) == 0u) {
    }
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW_Msk, RCC_CFGR_SW_PLL_HIDDEN);
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL_HIDDEN) {
    }
    LL_SetSystemCoreClock(SYSTEM_CLOCK_HZ);
}

static void timer_init(void)
{
    /*
     * Cortex-M0+ 没有 STM32F103 所用的 DWT 周期计数器。SysTick 以最大
     * 重装值自由运行；主循环每条 PIC 指令都会读取它，远快于 349 ms 的
     * 回绕周期，因此可无歧义累加为 32 位向上计数时间轴。
     */
    SysTick->LOAD = SYSTICK_MASK;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    timer_last_value = SysTick->VAL & SYSTICK_MASK;
    timer_elapsed_ticks = 0u;
}

static uint32_t timer_now(void)
{
    uint32_t current = SysTick->VAL & SYSTICK_MASK;
    timer_elapsed_ticks += (timer_last_value - current) & SYSTICK_MASK;
    timer_last_value = current;
    return timer_elapsed_ticks;
}

static void platform_set_pin_mode(void *context, unsigned pin,
                                  PicPlatformPinMode mode)
{
    const Py32PicPinMap *mapping;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return;
    mapping = &PY32_PIC_PIN_MAP[pin];
    if (mapping->gpio == NULL) return;

    LL_IOP_GRP1_EnableClock(mapping->clock_mask);
    LL_GPIO_SetPinMode(mapping->gpio, mapping->pin,
        mode == PIC_PLATFORM_PIN_OUTPUT ?
        LL_GPIO_MODE_OUTPUT : LL_GPIO_MODE_INPUT);
    if (mode == PIC_PLATFORM_PIN_OUTPUT) {
        LL_GPIO_SetPinOutputType(mapping->gpio, mapping->pin,
                                 LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(mapping->gpio, mapping->pin,
                            LL_GPIO_SPEED_FREQ_LOW);
    }
    LL_GPIO_SetPinPull(mapping->gpio, mapping->pin, LL_GPIO_PULL_NO);
}

static void platform_write_pin(void *context, unsigned pin, bool high)
{
    const Py32PicPinMap *mapping;
    bool physical_high;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return;
    mapping = &PY32_PIC_PIN_MAP[pin];
    if (mapping->gpio == NULL) return;
    physical_high = mapping->active_low ? !high : high;
    if (physical_high) {
        LL_GPIO_SetOutputPin(mapping->gpio, mapping->pin);
    } else {
        LL_GPIO_ResetOutputPin(mapping->gpio, mapping->pin);
    }
}

static bool platform_read_pin(void *context, unsigned pin)
{
    const Py32PicPinMap *mapping;
    bool physical_high;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return true;
    mapping = &PY32_PIC_PIN_MAP[pin];
    if (mapping->gpio == NULL) return true;
    physical_high = LL_GPIO_IsInputPinSet(mapping->gpio, mapping->pin) != 0u;
    return mapping->active_low ? !physical_high : physical_high;
}

static const PicPlatformOps PY32_PLATFORM = {
    .context = NULL,
    .set_pin_mode = platform_set_pin_mode,
    .write_pin = platform_write_pin,
    .read_pin = platform_read_pin,
    .time_us = NULL
};

int main(void)
{
    unsigned gpio_poll_cycles = 0u;
    uint32_t next_deadline;

    system_clock_config();
    timer_init();
    pic10_init(&pic_cpu, &pic_firmware_image,
               PY32_PIC10F202 ?
               &PIC_DEVICE_PIC10F202 : &PIC_DEVICE_PIC10F200);
    pic_hardware_bridge_init(&gpio_bridge, &pic_cpu, &PY32_PLATFORM);
    pic_hardware_bridge_sync(&gpio_bridge);
    next_deadline = timer_now();

    for (;;) {
        uint8_t old_gpio_latch = pic_cpu.gpio_latch;
        unsigned instruction_cycles = pic10_step_cycles(&pic_cpu);
        uint32_t now;
        int32_t timing;

        gpio_poll_cycles += instruction_cycles;
        if (old_gpio_latch != pic_cpu.gpio_latch ||
            gpio_bridge.last_tris != pic_cpu.tris_gpio ||
            gpio_poll_cycles >= GPIO_INPUT_POLL_CYCLES) {
            pic_hardware_bridge_sync(&gpio_bridge);
            gpio_poll_cycles = 0u;
        }

        next_deadline += instruction_cycles * PY32_TICKS_PER_PIC_CYCLE;
        now = timer_now();
        timing = (int32_t)(now - next_deadline);
        if (timing < 0) {
            do {
                now = timer_now();
            } while ((int32_t)(now - next_deadline) < 0);
        } else if ((uint32_t)timing > SCHEDULER_MAX_LAG_TICKS) {
            /* 落后超过 10 ms 时放弃无法偿还的时间欠账，但不跳过 PIC 指令。 */
            next_deadline = now;
        }
    }
}
