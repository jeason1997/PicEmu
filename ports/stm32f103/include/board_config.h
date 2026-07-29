#ifndef STM32_BOARD_CONFIG_H
#define STM32_BOARD_CONFIG_H

#include "stm32f103_regs.h"

/*
 * PIC GPIO到STM32引脚的集中映射。
 *
 * gpio为NULL表示该PIC引脚不映射到真实硬件。active_low为1时，
 * PIC逻辑高电平会转换成STM32物理低电平，适合Blue Pill板载LED。
 */
typedef struct {
    Stm32Gpio *gpio;
    unsigned pin;
    unsigned active_low;
} Stm32PicPinMap;

static const Stm32PicPinMap STM32_PIC_PIN_MAP[4] = {
    /* GP0 */ {GPIOC, 13u, 1u},
    /* GP1 */ {GPIOA,  0u, 0u},
    /* GP2 */ {GPIOA,  1u, 0u},
    /* GP3 */ {GPIOA,  2u, 0u}
};

#endif
