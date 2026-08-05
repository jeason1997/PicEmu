#ifndef PY32_BOARD_CONFIG_H
#define PY32_BOARD_CONFIG_H

#include "py32f0xx.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"

/*
 * PIC GPIO 到 PY32F002A 引脚的集中映射。
 *
 * GP0 沿用参考 Blink 工程的 PB1，便于直接观察最常用的 blink 示例。
 * 其余引脚避开 SWD 调试口；不需要的引脚可把 gpio 改成 NULL。active_low
 * 用于适配低电平点亮的 LED，而不改变 PIC 固件看到的逻辑电平。
 */
typedef struct {
    GPIO_TypeDef *gpio;
    uint32_t pin;
    uint32_t clock_mask;
    unsigned active_low;
} Py32PicPinMap;

static const Py32PicPinMap PY32_PIC_PIN_MAP[4] = {
    /* GP0 */ {GPIOB, LL_GPIO_PIN_1, LL_IOP_GRP1_PERIPH_GPIOB, 0u},
    /* GP1 */ {GPIOA, LL_GPIO_PIN_1, LL_IOP_GRP1_PERIPH_GPIOA, 0u},
    /* GP2 */ {GPIOA, LL_GPIO_PIN_2, LL_IOP_GRP1_PERIPH_GPIOA, 0u},
    /* GP3 */ {GPIOA, LL_GPIO_PIN_3, LL_IOP_GRP1_PERIPH_GPIOA, 0u}
};

#endif
