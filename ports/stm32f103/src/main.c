#include "board_config.h"
#include "debug_uart.h"
#include "firmware.h"
#include "picemu/core/pic10_cpu.h"
#include "picemu/platform/gpio_bridge.h"
#include "stm32f103_regs.h"

#include <stdbool.h>
#include <stdint.h>

#ifndef STM32_CLOCK_MHZ
#define STM32_CLOCK_MHZ 128
#endif

#if STM32_CLOCK_MHZ != 16 && STM32_CLOCK_MHZ != 24 && \
    STM32_CLOCK_MHZ != 32 && STM32_CLOCK_MHZ != 40 && \
    STM32_CLOCK_MHZ != 48 && STM32_CLOCK_MHZ != 56 && \
    STM32_CLOCK_MHZ != 64 && STM32_CLOCK_MHZ != 72 && \
    STM32_CLOCK_MHZ != 128
#error "Unsupported STM32_CLOCK_MHZ"
#endif

#define STM32_PLL_MULTIPLIER (STM32_CLOCK_MHZ / 8u)
#define STM32_CORE_HZ_VALUE  (STM32_CLOCK_MHZ * 1000000u)

#if STM32_CLOCK_MHZ <= 24
#define STM32_FLASH_LATENCY FLASH_ACR_LATENCY_0
#elif STM32_CLOCK_MHZ <= 48
#define STM32_FLASH_LATENCY FLASH_ACR_LATENCY_1
#else
#define STM32_FLASH_LATENCY FLASH_ACR_LATENCY_2
#endif

#if STM32_CLOCK_MHZ <= 32
#define STM32_APB1_PRESCALER 0u
#elif STM32_CLOCK_MHZ <= 72
#define STM32_APB1_PRESCALER RCC_CFGR_PPRE1_DIV2
#else
#define STM32_APB1_PRESCALER RCC_CFGR_PPRE1_DIV4
#endif

#if STM32_CLOCK_MHZ <= 72
#define STM32_APB2_PRESCALER 0u
#define STM32_APB2_HZ_VALUE  STM32_CORE_HZ_VALUE
#else
#define STM32_APB2_PRESCALER RCC_CFGR_PPRE2_DIV2
#define STM32_APB2_HZ_VALUE  (STM32_CORE_HZ_VALUE / 2u)
#endif

enum {
    STM32_CORE_HZ = STM32_CORE_HZ_VALUE,
    STM32_APB2_HZ = STM32_APB2_HZ_VALUE,
    PIC_OSCILLATOR_HZ = 4000000u,
    PIC_CYCLES_PER_SECOND = PIC_OSCILLATOR_HZ / 4u,
    STM32_TICKS_PER_PIC_CYCLE = STM32_CORE_HZ / PIC_CYCLES_PER_SECOND,
    PIC_GPIO_COUNT = 4u,
    GPIO_INPUT_POLL_CYCLES = 64u,
    /*
     * 当解释器跟不上实时速度时，不允许截止时间无限落后。
     * 串口边沿日志本身约占3ms，因此保留10ms追赶窗口，使日志结束后
     * 模拟器能够追回这段时间；超过10ms才重新锚定到当前DWT，避免累计
     * 落后跨过2^31后被误判为“尚未到截止时间”。
     */
    SCHEDULER_MAX_LAG_TICKS = STM32_CORE_HZ / 100u
};

static Pic10Cpu pic_cpu;
static PicHardwareBridge gpio_bridge;
static bool gp0_log_initialized;
static bool gp0_last_level;
static uint32_t timestamp_last_dwt;
static uint32_t timestamp_elapsed_ms;
static uint32_t timestamp_remainder_ticks;

static void clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {
    }
    debug_uart_puts("[boot] HSI ready\n");

    FLASH_REGS->ACR = FLASH_ACR_PRFTBE | STM32_FLASH_LATENCY;
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0) {
    }
    debug_uart_puts("[boot] HSE ready\n");
    /*
     * 此时USART1仍依赖8MHz APB2。先只配置PLL源和倍频，不提前设置
     * APB2分频，否则“PLL locked”日志会暂时从115200降成57600波特。
     */
    RCC->CFGR = RCC_CFGR_PLLSRC_HSE |
                RCC_CFGR_PLLMUL(STM32_PLL_MULTIPLIER);
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
    }
    debug_uart_puts("[boot] PLL locked\n");
    debug_uart_flush();
    /*
     * 串口最后一个停止位发送完毕后再设置总线分频。随后立即切换SYSCLK，
     * 并按照所选频率对应的APB2时钟重新配置USART1。
     */
    RCC->CFGR |= STM32_APB1_PRESCALER | STM32_APB2_PRESCALER;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & (3u << 2)) != RCC_CFGR_SWS_PLL) {
    }
    debug_uart_init(STM32_APB2_HZ);
    debug_uart_puts("[boot] system clock ");
    debug_uart_put_u64(STM32_CLOCK_MHZ);
#if STM32_CLOCK_MHZ > 72
    debug_uart_puts(" MHz (overclock)\n");
#else
    debug_uart_puts(" MHz\n");
#endif
}

static void cycle_counter_init(void)
{
    CORE_DEMCR |= CORE_DEMCR_TRCENA;
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
    timestamp_last_dwt = DWT_CYCCNT;
    timestamp_elapsed_ms = 0;
    timestamp_remainder_ticks = 0;
}

static uint32_t timestamp_ms(void)
{
    uint32_t now = DWT_CYCCNT;
    uint32_t delta = now - timestamp_last_dwt;
    uint32_t ticks_per_ms = STM32_CORE_HZ / 1000u;

    /*
     * 无符号减法能够正确处理32位DWT计数器回绕。只要相邻两次日志间隔
     * 不超过一次完整回绕，delta就是准确间隔。
     * 每次先把较小的delta换算成毫秒，避免64位累计跨过2^32时受到工具链
     * 或超频运行稳定性的影响；余数保留下来，不会持续丢失小数毫秒。
     */
    timestamp_last_dwt = now;
    timestamp_elapsed_ms += delta / ticks_per_ms;
    timestamp_remainder_ticks += delta % ticks_per_ms;
    timestamp_elapsed_ms += timestamp_remainder_ticks / ticks_per_ms;
    timestamp_remainder_ticks %= ticks_per_ms;
    return timestamp_elapsed_ms;
}

static void gpio_enable_clock(Stm32Gpio *gpio)
{
    if (gpio == GPIOA) RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    else if (gpio == GPIOB) RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    else if (gpio == GPIOC) RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
}

static void gpio_set_mode(Stm32Gpio *gpio, unsigned pin,
                          PicPlatformPinMode mode)
{
    volatile uint32_t *config_reg;
    uint32_t shift;
    uint32_t config;

    gpio_enable_clock(gpio);
    config_reg = pin < 8u ? &gpio->CRL : &gpio->CRH;
    shift = (pin & 7u) * 4u;
    config = *config_reg;
    config &= ~(0xFu << shift);
    if (mode == PIC_PLATFORM_PIN_OUTPUT) {
        config |= 0x2u << shift; /* 2MHz通用推挽输出。 */
    }
    *config_reg = config;
}

static void platform_set_pin_mode(void *context, unsigned pin,
                                  PicPlatformPinMode mode)
{
    const Stm32PicPinMap *mapping;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return;
    mapping = &STM32_PIC_PIN_MAP[pin];
    if (mapping->gpio == 0) return;
    gpio_set_mode(mapping->gpio, mapping->pin, mode);
}

static void platform_write_pin(void *context, unsigned pin, bool high)
{
    const Stm32PicPinMap *mapping;
    bool physical_high;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return;
    mapping = &STM32_PIC_PIN_MAP[pin];
    if (mapping->gpio == 0) return;

    physical_high = mapping->active_low ? !high : high;
    mapping->gpio->BSRR = physical_high
        ? (1u << mapping->pin) : (1u << (mapping->pin + 16u));

    if (pin == 0u &&
        (!gp0_log_initialized || gp0_last_level != high)) {
        gp0_log_initialized = true;
        gp0_last_level = high;
        debug_uart_puts("[gpio t=");
        debug_uart_put_u64(timestamp_ms());
        debug_uart_puts(" ms pic=");
        debug_uart_put_u64(pic_cpu.cycles);
        debug_uart_puts(high ? "] GP0=1\n" : "] GP0=0\n");
    }
}

static bool platform_read_pin(void *context, unsigned pin)
{
    const Stm32PicPinMap *mapping;
    (void)context;
    if (pin >= PIC_GPIO_COUNT) return true;
    mapping = &STM32_PIC_PIN_MAP[pin];
    if (mapping->gpio != 0) {
        bool physical_high =
            (mapping->gpio->IDR & (1u << mapping->pin)) != 0;
        return mapping->active_low ? !physical_high : physical_high;
    }
    /* 未映射引脚视为悬空高电平。 */
    return true;
}

static const PicPlatformOps STM32_PLATFORM = {
    .context = NULL,
    .set_pin_mode = platform_set_pin_mode,
    .write_pin = platform_write_pin,
    .read_pin = platform_read_pin,
    .time_us = NULL
};

int main(void)
{
    unsigned gpio_poll_cycles = 0;
    uint32_t next_deadline;

    debug_uart_init(8000000u);
    debug_uart_puts("\n[boot] reset handler entered\n");
    clock_init();
    cycle_counter_init();
    debug_uart_puts("[boot] DWT cycle counter enabled\n");

    pic10_init(&pic_cpu, &pic_firmware_image, &PIC_DEVICE_PIC10F200);
    pic_hardware_bridge_init(&gpio_bridge, &pic_cpu, &STM32_PLATFORM);
    debug_uart_puts("[boot] PIC10F200 initialized\n");
    next_deadline = DWT_CYCCNT;
    pic_hardware_bridge_sync(&gpio_bridge);

    for (;;) {
        /*
         * PIC10F200在4MHz下每秒有100万个指令周期。DWT以所选核心频率
         * 计数，因此一个PIC周期对应STM32_CLOCK_MHZ个STM32周期。
         * 两周期PIC指令会正确扣除
         * 两倍时间。若STM32来得及，会等待到精确截止时间；若解释执行
         * 本身超过预算，则不再等待，以处理器能够达到的最高速度继续。
         */
        uint8_t old_gpio_latch = pic_cpu.gpio_latch;
        unsigned instruction_cycles = pic10_step_cycles(&pic_cpu);

        gpio_poll_cycles += instruction_cycles;
        if (old_gpio_latch != pic_cpu.gpio_latch ||
            gpio_bridge.last_tris != pic_cpu.tris_gpio ||
            gpio_poll_cycles >= GPIO_INPUT_POLL_CYCLES) {
            pic_hardware_bridge_sync(&gpio_bridge);
            gpio_poll_cycles = 0;
        }
        next_deadline +=
            instruction_cycles * STM32_TICKS_PER_PIC_CYCLE;
        {
            int32_t timing = (int32_t)(DWT_CYCCNT - next_deadline);

            if (timing < 0) {
                while ((int32_t)(DWT_CYCCNT - next_deadline) < 0) {
                }
            } else if ((uint32_t)timing > SCHEDULER_MAX_LAG_TICKS) {
                /*
                 * 解释器已经落后，继续保留历史截止时间无法追赶，只会让
                 * 32位回绕判断最终失效。重同步不会让解释器变快或丢指令，
                 * 只是取消无法偿还的真实时间欠账。
                 */
                next_deadline = DWT_CYCCNT;
            }
        }
    }
}
