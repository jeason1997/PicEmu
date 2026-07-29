#include "board_config.h"
#include "firmware.h"
#include "picemu/core/pic10_cpu.h"
#include "picemu/platform/gpio_bridge.h"
#include "stm32f103_regs.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    STM32_CORE_HZ = 64000000u,
    PIC_OSCILLATOR_HZ = 4000000u,
    PIC_CYCLES_PER_SECOND = PIC_OSCILLATOR_HZ / 4u,
    STM32_TICKS_PER_PIC_CYCLE = STM32_CORE_HZ / PIC_CYCLES_PER_SECOND,
    PIC_GPIO_COUNT = 4u
};

static Pic10Cpu pic_cpu;
static PicHardwareBridge gpio_bridge;

static void clock_init_64mhz(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {
    }

    FLASH_REGS->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    /*
     * HSI/2 = 4MHz，PLL乘16得到64MHz；APB1设置为32MHz，
     * 未超过STM32F103数据手册规定的36MHz上限。
     */
    RCC->CFGR = RCC_CFGR_PLLMUL16 | RCC_CFGR_PPRE1_DIV2;
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
    }
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & (3u << 2)) != RCC_CFGR_SWS_PLL) {
    }
}

static void cycle_counter_init(void)
{
    CORE_DEMCR |= CORE_DEMCR_TRCENA;
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
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
    uint32_t next_deadline;

    clock_init_64mhz();
    cycle_counter_init();

    pic10_init(&pic_cpu, &pic_firmware_image, &PIC_DEVICE_PIC10F200);
    pic_hardware_bridge_init(&gpio_bridge, &pic_cpu, &STM32_PLATFORM);
    next_deadline = DWT_CYCCNT;

    for (;;) {
        /*
         * PIC10F200在4MHz下每秒有100万个指令周期。DWT以64MHz计数，
         * 因此一个PIC周期对应64个STM32周期。两周期PIC指令会正确扣除
         * 两倍时间。若STM32来得及，会等待到精确截止时间；若解释执行
         * 本身超过预算，则不再等待，以处理器能够达到的最高速度继续。
         */
        Pic10StepResult result;
        pic_hardware_bridge_sync(&gpio_bridge);
        result = pic10_step(&pic_cpu);
        pic_hardware_bridge_sync(&gpio_bridge);
        next_deadline +=
            result.instruction_cycles * STM32_TICKS_PER_PIC_CYCLE;
        while ((int32_t)(DWT_CYCCNT - next_deadline) < 0) {
        }
    }
}
