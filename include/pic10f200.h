#ifndef PIC10F200_H
#define PIC10F200_H

#include "hex_loader.h"

#include <stdbool.h>
#include <stdint.h>

/* PIC10F200 数据存储器中的特殊功能寄存器地址。 */
enum {
    PIC10_INDF   = 0x00,
    PIC10_TMR0   = 0x01,
    PIC10_PCL    = 0x02,
    PIC10_STATUS = 0x03,
    PIC10_FSR    = 0x04,
    PIC10_OSCCAL = 0x05,
    PIC10_GPIO   = 0x06
};

/* STATUS 寄存器位。 */
enum {
    PIC10_STATUS_C  = 0,
    PIC10_STATUS_DC = 1,
    PIC10_STATUS_Z  = 2,
    PIC10_STATUS_PD = 3,
    PIC10_STATUS_TO = 4,
    PIC10_STATUS_PA0 = 5
};

typedef enum {
    PIC10_RESET_POWER_ON,
    PIC10_RESET_MCLR,
    PIC10_RESET_WATCHDOG
} Pic10ResetReason;

typedef struct {
    uint16_t program[PIC10F200_PROGRAM_WORDS];

    /*
     * 为便于学习，这里保留完整的 32 字节地址空间。
     * 0x00~0x06 是 SFR；PIC10F200 的通用 RAM 位于 0x10~0x1F。
     */
    uint8_t ram[32];

    uint8_t w;
    uint16_t pc;

    uint16_t stack[2];
    uint8_t stack_pointer;

    uint8_t tris_gpio;
    uint8_t gpio_latch;
    uint8_t gpio_inputs;
    uint8_t gpio_input_mask;
    uint8_t option;

    uint32_t timer0_prescaler;
    unsigned timer0_write_inhibit;
    uint32_t watchdog_counter;
    uint32_t watchdog_base_period;
    uint16_t config_word;
    uint64_t cycles;

    bool sleeping;
    bool stopped;
    bool watchdog_enabled;
    Pic10ResetReason last_reset;
    const char *stop_reason;
} Pic10F200;

typedef struct {
    uint16_t executed_pc;
    uint16_t instruction;
    uint8_t old_gpio;
    uint8_t new_gpio;
    bool gpio_changed;
    bool reset_occurred;
    bool woke_from_sleep;
    unsigned instruction_cycles;
} Pic10StepResult;

void pic10f200_init(Pic10F200 *cpu, const HexImage *image);
void pic10f200_reset(Pic10F200 *cpu, Pic10ResetReason reason);

/*
 * 执行一条指令。返回的信息包含该指令消耗的周期数及 GPIO 变化。
 * 普通指令为 1 个周期，跳转以及真正发生的“跳过”为 2 个周期。
 */
Pic10StepResult pic10f200_step(Pic10F200 *cpu);

/* 获取引脚上实际可见的 GPIO 电平，而不是单纯返回输出锁存器。 */
uint8_t pic10f200_gpio_value(const Pic10F200 *cpu);

/* 由外部电路驱动或释放一个输入引脚。GP3始终只能作为输入。 */
void pic10f200_drive_pin(Pic10F200 *cpu, unsigned pin,
                         bool driven, bool high);

/* 调试器使用的只读寄存器访问接口，遵循SFR和INDF读取规则。 */
uint8_t pic10f200_read_register(Pic10F200 *cpu, uint8_t address);

#endif
