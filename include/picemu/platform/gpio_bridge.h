#ifndef PIC_PLATFORM_H
#define PIC_PLATFORM_H

#include "picemu/core/pic10_cpu.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PIC_PLATFORM_PIN_INPUT,
    PIC_PLATFORM_PIN_OUTPUT
} PicPlatformPinMode;

/*
 * 平台只需提供GPIO和微秒计时回调。核心中不包含STM32 HAL、SDL或操作
 * 系统头文件，因此同一份核心源码可以由ARM GCC直接编译。
 */
typedef struct {
    void *context;
    void (*set_pin_mode)(void *context, unsigned pin,
                         PicPlatformPinMode mode);
    void (*write_pin)(void *context, unsigned pin, bool high);
    bool (*read_pin)(void *context, unsigned pin);
    uint64_t (*time_us)(void *context);
} PicPlatformOps;

typedef struct {
    Pic10Cpu *cpu;
    const PicPlatformOps *ops;
    uint8_t last_tris;
    uint8_t last_output;
    bool initialized;
} PicHardwareBridge;

void pic_hardware_bridge_init(PicHardwareBridge *bridge,
                              Pic10Cpu *cpu,
                              const PicPlatformOps *ops);

/*
 * 同步一次虚拟PIC引脚与真实平台GPIO：
 * - TRIS输出：配置平台为输出并写入PIC锁存值；
 * - TRIS输入：配置平台为输入并把平台电平送回PIC核心。
 */
void pic_hardware_bridge_sync(PicHardwareBridge *bridge);

#endif
