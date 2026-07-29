#include "pic10f200.h"

#include <string.h>

#define BIT(value, bit) (((value) >> (bit)) & 1u)

static void set_status_bit(Pic10F200 *cpu, unsigned bit, bool set)
{
    if (set) {
        cpu->ram[PIC10_STATUS] |= (uint8_t)(1u << bit);
    } else {
        cpu->ram[PIC10_STATUS] &= (uint8_t)~(1u << bit);
    }
}

static void update_zero(Pic10F200 *cpu, uint8_t value)
{
    set_status_bit(cpu, PIC10_STATUS_Z, value == 0);
}

uint8_t pic10f200_gpio_value(const Pic10F200 *cpu)
{
    /*
     * TRIS 位为 0 时是输出，读到输出锁存值；
     * TRIS 位为 1 时是输入，读到外部输入值。
     * PIC10F200 的 GP3 只能作为输入，所以始终把方向位 3 置 1。
     */
    uint8_t tris = (uint8_t)(cpu->tris_gpio | 0x08u);
    uint8_t weak_pullups = (cpu->option & 0x40u) == 0
        ? (uint8_t)(~cpu->gpio_input_mask & 0x0Fu) : 0;
    uint8_t inputs = (uint8_t)(cpu->gpio_inputs | weak_pullups);
    return (uint8_t)(((cpu->gpio_latch & (uint8_t)~tris) |
                      (inputs & tris)) & 0x0Fu);
}

uint8_t pic10f200_read_register(Pic10F200 *cpu, uint8_t address)
{
    address &= 0x1Fu;

    if (address == PIC10_INDF) {
        uint8_t indirect = (uint8_t)(cpu->ram[PIC10_FSR] & 0x1Fu);
        /* FSR 指向 INDF 自己时，PIC 规定读出 0。 */
        return indirect == PIC10_INDF
            ? 0 : pic10f200_read_register(cpu, indirect);
    }
    if (address == PIC10_PCL) {
        return (uint8_t)(cpu->pc & 0xFFu);
    }
    if (address == PIC10_GPIO) {
        return pic10f200_gpio_value(cpu);
    }
    return cpu->ram[address];
}

static void write_register(Pic10F200 *cpu, uint8_t address, uint8_t value)
{
    address &= 0x1Fu;

    if (address == PIC10_INDF) {
        uint8_t indirect = (uint8_t)(cpu->ram[PIC10_FSR] & 0x1Fu);
        if (indirect != PIC10_INDF) {
            write_register(cpu, indirect, value);
        }
        return;
    }
    if (address == PIC10_PCL) {
        /*
         * 写 PCL 会改变下一条指令地址。PIC10F200 只有 9 位 PC，
         * 最高位来自 STATUS.PA0。
         */
        cpu->pc = (uint16_t)((BIT(cpu->ram[PIC10_STATUS],
                                  PIC10_STATUS_PA0) << 8) | value);
        return;
    }
    if (address == PIC10_TMR0) {
        cpu->ram[PIC10_TMR0] = value;
        cpu->timer0_prescaler = 0;
        /*
         * 数据手册规定写TMR0后的两个指令周期禁止递增。
         * 当前写指令还会经过本次tick，所以内部计数设为3。
         */
        cpu->timer0_write_inhibit = 3;
        return;
    }
    if (address == PIC10_STATUS) {
        /*
         * TO和PD是只读状态位；位6、7在PIC10F200中未实现。
         * C/DC/Z及页面选择位PA0可以由软件写入。
         */
        cpu->ram[PIC10_STATUS] =
            (uint8_t)((cpu->ram[PIC10_STATUS] & 0x18u) |
                      (value & 0x27u));
        return;
    }
    if (address == PIC10_GPIO) {
        cpu->gpio_latch = (uint8_t)(value & 0x0Fu);
        cpu->ram[PIC10_GPIO] = cpu->gpio_latch;
        return;
    }

    cpu->ram[address] = value;
}

static void write_destination(Pic10F200 *cpu, uint8_t file,
                              bool destination_file, uint8_t value)
{
    if (destination_file) {
        write_register(cpu, file, value);
    } else {
        cpu->w = value;
    }
}

static void push_stack(Pic10F200 *cpu, uint16_t address)
{
    cpu->stack[cpu->stack_pointer] = address;
    cpu->stack_pointer = (uint8_t)((cpu->stack_pointer + 1u) & 1u);
}

static uint16_t pop_stack(Pic10F200 *cpu)
{
    cpu->stack_pointer = (uint8_t)((cpu->stack_pointer - 1u) & 1u);
    return cpu->stack[cpu->stack_pointer];
}

static void tick_timer0(Pic10F200 *cpu, unsigned instruction_cycles)
{
    unsigned i;

    /* OPTION.T0CS=1 表示外部计数；本模拟器暂时没有外部 T0CKI 事件。 */
    if (BIT(cpu->option, 5)) {
        return;
    }

    for (i = 0; i < instruction_cycles; ++i) {
        if (cpu->timer0_write_inhibit > 0) {
            --cpu->timer0_write_inhibit;
            continue;
        }
        /* PSA=1 时预分频器分配给 WDT，Timer0 每周期直接加一。 */
        if (BIT(cpu->option, 3)) {
            ++cpu->ram[PIC10_TMR0];
        } else {
            uint32_t divider = 1u << ((cpu->option & 0x07u) + 1u);
            ++cpu->timer0_prescaler;
            if (cpu->timer0_prescaler >= divider) {
                cpu->timer0_prescaler = 0;
                ++cpu->ram[PIC10_TMR0];
            }
        }
    }
}

static void timer0_external_pulse(Pic10F200 *cpu)
{
    if (BIT(cpu->option, 3)) {
        ++cpu->ram[PIC10_TMR0];
    } else {
        uint32_t divider = 1u << ((cpu->option & 7u) + 1u);
        if (++cpu->timer0_prescaler >= divider) {
            cpu->timer0_prescaler = 0;
            ++cpu->ram[PIC10_TMR0];
        }
    }
}

void pic10f200_reset(Pic10F200 *cpu, Pic10ResetReason reason)
{
    uint8_t preserved_gpr[16];

    memcpy(preserved_gpr, &cpu->ram[0x10], sizeof(preserved_gpr));
    memset(cpu->ram, 0, sizeof(cpu->ram));
    if (reason != PIC10_RESET_POWER_ON) {
        memcpy(&cpu->ram[0x10], preserved_gpr, sizeof(preserved_gpr));
    }

    cpu->pc = 0;
    cpu->w = 0;
    cpu->stack[0] = 0;
    cpu->stack[1] = 0;
    cpu->stack_pointer = 0;
    cpu->tris_gpio = 0x0Fu;
    cpu->option = 0xFFu;
    cpu->gpio_latch = 0;
    cpu->timer0_prescaler = 0;
    cpu->timer0_write_inhibit = 0;
    cpu->watchdog_counter = 0;
    cpu->sleeping = false;
    cpu->stopped = false;
    cpu->stop_reason = NULL;
    cpu->last_reset = reason;

    /* POR/MCLR后TO、PD为1；WDT运行时复位使TO清零。 */
    cpu->ram[PIC10_STATUS] =
        (uint8_t)((1u << PIC10_STATUS_TO) | (1u << PIC10_STATUS_PD));
    if (reason == PIC10_RESET_WATCHDOG) {
        set_status_bit(cpu, PIC10_STATUS_TO, false);
    }
}

void pic10f200_init(Pic10F200 *cpu, const HexImage *image)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->device = &PIC_DEVICE_PIC10F200;
    memcpy(cpu->program, image->program, sizeof(cpu->program));
    cpu->config_word = image->config_present ? image->config_word : 0x0FFFu;
    cpu->watchdog_enabled = BIT(cpu->config_word, 2) != 0;
    /*
     * PIC10F200 WDT标称基础超时约18 ms。模拟器以默认4 MHz振荡器下的
     * 1 us指令周期为单位，因此基础周期取18000。它是数字近似值，不模拟
     * 芯片、温度和电压造成的WDT振荡器离散性。
     */
    cpu->watchdog_base_period = 18000;
    pic10f200_reset(cpu, PIC10_RESET_POWER_ON);
}

static bool tick_watchdog(Pic10F200 *cpu, unsigned cycles)
{
    uint32_t divider = 1;
    uint64_t timeout;

    if (!cpu->watchdog_enabled) {
        return false;
    }
    if (BIT(cpu->option, 3)) {
        divider = 1u << (cpu->option & 7u);
    }
    timeout = (uint64_t)cpu->watchdog_base_period * divider;
    cpu->watchdog_counter += cycles;
    if (cpu->watchdog_counter < timeout) {
        return false;
    }

    cpu->watchdog_counter = 0;
    if (cpu->sleeping) {
        cpu->sleeping = false;
        set_status_bit(cpu, PIC10_STATUS_TO, false);
        set_status_bit(cpu, PIC10_STATUS_PD, false);
    } else {
        pic10f200_reset(cpu, PIC10_RESET_WATCHDOG);
    }
    return true;
}

void pic10f200_drive_pin(Pic10F200 *cpu, unsigned pin,
                         bool driven, bool high)
{
    uint8_t mask;
    uint8_t old_level;
    uint8_t new_level;

    if (pin >= 4) {
        return;
    }
    mask = (uint8_t)(1u << pin);
    old_level = pic10f200_gpio_value(cpu);

    if (driven) {
        cpu->gpio_input_mask |= mask;
        if (high) {
            cpu->gpio_inputs |= mask;
        } else {
            cpu->gpio_inputs &= (uint8_t)~mask;
        }
    } else {
        cpu->gpio_input_mask &= (uint8_t)~mask;
        cpu->gpio_inputs &= (uint8_t)~mask;
    }

    new_level = pic10f200_gpio_value(cpu);
    if (pin == 2 && BIT(cpu->option, 5) &&
        ((old_level ^ new_level) & mask) != 0) {
        bool rising = (new_level & mask) != 0;
        bool count_falling = BIT(cpu->option, 4) != 0;
        if (rising != count_falling) {
            timer0_external_pulse(cpu);
        }
    }

    /* GP0、GP1或GP3变化，并且GPWU=0时，可以从Sleep唤醒。 */
    if (cpu->sleeping && (cpu->option & 0x80u) == 0 &&
        pin != 2 && ((old_level ^ new_level) & mask) != 0) {
        cpu->sleeping = false;
    }
}

Pic10StepResult pic10f200_step(Pic10F200 *cpu)
{
    Pic10StepResult result = {0};
    uint16_t instruction;
    uint8_t file;
    bool destination_file;
    uint8_t old_gpio;
    uint8_t operand;
    uint8_t value;
    unsigned cycles = 1;

    old_gpio = pic10f200_gpio_value(cpu);

    if (cpu->stopped) {
        result.old_gpio = old_gpio;
        result.new_gpio = old_gpio;
        return result;
    }
    if (cpu->sleeping) {
        cpu->cycles += 1;
        result.woke_from_sleep = tick_watchdog(cpu, 1);
        result.old_gpio = old_gpio;
        result.new_gpio = pic10f200_gpio_value(cpu);
        result.gpio_changed = result.old_gpio != result.new_gpio;
        result.instruction_cycles = 1;
        return result;
    }
    if (cpu->pc >= PIC10F200_PROGRAM_WORDS) {
        cpu->stopped = true;
        cpu->stop_reason = "程序计数器超出 PIC10F200 程序空间";
        result.old_gpio = old_gpio;
        result.new_gpio = old_gpio;
        return result;
    }

    result.executed_pc = cpu->pc;
    instruction = (uint16_t)(cpu->program[cpu->pc] & 0x0FFFu);
    result.instruction = instruction;

    /*
     * PIC 在执行指令前先把 PC 指向下一条指令。这样 CALL 压栈的就是
     * 返回地址，读取 PCL 时也能自然得到流水线中的 PC 值。
     */
    cpu->pc = (uint16_t)((cpu->pc + 1u) & 0x01FFu);
    file = (uint8_t)(instruction & 0x1Fu);
    destination_file = BIT(instruction, 5) != 0;

    if (instruction == 0x000u) {                 /* NOP */
        /* 什么也不做。 */
    } else if (instruction == 0x002u) {          /* OPTION */
        cpu->option = cpu->w;
        cpu->timer0_prescaler = 0;
        cpu->watchdog_counter = 0;
    } else if (instruction == 0x003u) {          /* SLEEP */
        cpu->sleeping = true;
        cpu->watchdog_counter = 0;
        set_status_bit(cpu, PIC10_STATUS_PD, false);
        set_status_bit(cpu, PIC10_STATUS_TO, true);
    } else if (instruction == 0x004u) {          /* CLRWDT */
        cpu->watchdog_counter = 0;
        set_status_bit(cpu, PIC10_STATUS_PD, true);
        set_status_bit(cpu, PIC10_STATUS_TO, true);
    } else if ((instruction & 0xFF8u) == 0x000u &&
               (instruction & 0x007u) >= 5u) {   /* TRIS f */
        if ((instruction & 0x007u) == PIC10_GPIO) {
            cpu->tris_gpio = (uint8_t)((cpu->w & 0x0Fu) | 0x08u);
        }
    } else if ((instruction & 0xFE0u) == 0x020u) { /* MOVWF f */
        write_register(cpu, file, cpu->w);
    } else if ((instruction & 0xFE0u) == 0x040u) { /* CLRW */
        cpu->w = 0;
        update_zero(cpu, cpu->w);
    } else if ((instruction & 0xFE0u) == 0x060u) { /* CLRF f */
        write_register(cpu, file, 0);
        update_zero(cpu, 0);
    } else if ((instruction & 0xFC0u) == 0x080u) { /* SUBWF f,d */
        unsigned lhs;
        unsigned rhs;
        unsigned difference;

        operand = pic10f200_read_register(cpu, file);
        lhs = operand;
        rhs = cpu->w;
        difference = (lhs - rhs) & 0xFFu;
        value = (uint8_t)difference;
        write_destination(cpu, file, destination_file, value);
        set_status_bit(cpu, PIC10_STATUS_C, lhs >= rhs);
        set_status_bit(cpu, PIC10_STATUS_DC,
                       (lhs & 0x0Fu) >= (rhs & 0x0Fu));
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x0C0u) { /* DECF f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) - 1u);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x100u) { /* IORWF f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) | cpu->w);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x140u) { /* ANDWF f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) & cpu->w);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x180u) { /* XORWF f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) ^ cpu->w);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x1C0u) { /* ADDWF f,d */
        unsigned sum;

        operand = pic10f200_read_register(cpu, file);
        sum = (unsigned)operand + cpu->w;
        value = (uint8_t)sum;
        write_destination(cpu, file, destination_file, value);
        set_status_bit(cpu, PIC10_STATUS_C, sum > 0xFFu);
        set_status_bit(cpu, PIC10_STATUS_DC,
                       ((operand & 0x0Fu) + (cpu->w & 0x0Fu)) > 0x0Fu);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x200u) { /* MOVF f,d */
        value = pic10f200_read_register(cpu, file);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x240u) { /* COMF f,d */
        value = (uint8_t)~pic10f200_read_register(cpu, file);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x280u) { /* INCF f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) + 1u);
        write_destination(cpu, file, destination_file, value);
        update_zero(cpu, value);
    } else if ((instruction & 0xFC0u) == 0x2C0u) { /* DECFSZ f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) - 1u);
        write_destination(cpu, file, destination_file, value);
        if (value == 0) {
            cpu->pc = (uint16_t)((cpu->pc + 1u) & 0x01FFu);
            cycles = 2;
        }
    } else if ((instruction & 0xFC0u) == 0x300u) { /* RRF f,d */
        operand = pic10f200_read_register(cpu, file);
        value = (uint8_t)((operand >> 1) |
                          (BIT(cpu->ram[PIC10_STATUS],
                               PIC10_STATUS_C) << 7));
        set_status_bit(cpu, PIC10_STATUS_C, (operand & 1u) != 0);
        write_destination(cpu, file, destination_file, value);
    } else if ((instruction & 0xFC0u) == 0x340u) { /* RLF f,d */
        operand = pic10f200_read_register(cpu, file);
        value = (uint8_t)((operand << 1) |
                          BIT(cpu->ram[PIC10_STATUS], PIC10_STATUS_C));
        set_status_bit(cpu, PIC10_STATUS_C, (operand & 0x80u) != 0);
        write_destination(cpu, file, destination_file, value);
    } else if ((instruction & 0xFC0u) == 0x380u) { /* SWAPF f,d */
        operand = pic10f200_read_register(cpu, file);
        value = (uint8_t)((operand << 4) | (operand >> 4));
        write_destination(cpu, file, destination_file, value);
    } else if ((instruction & 0xFC0u) == 0x3C0u) { /* INCFSZ f,d */
        value = (uint8_t)(pic10f200_read_register(cpu, file) + 1u);
        write_destination(cpu, file, destination_file, value);
        if (value == 0) {
            cpu->pc = (uint16_t)((cpu->pc + 1u) & 0x01FFu);
            cycles = 2;
        }
    } else if ((instruction & 0xF00u) >= 0x400u &&
               (instruction & 0xF00u) <= 0x700u) {
        unsigned operation = (instruction >> 8) & 0x03u;
        unsigned bit = (instruction >> 5) & 0x07u;
        uint8_t mask = (uint8_t)(1u << bit);

        operand = pic10f200_read_register(cpu, file);
        if (operation == 0) {                    /* BCF f,b */
            write_register(cpu, file, (uint8_t)(operand & ~mask));
        } else if (operation == 1) {             /* BSF f,b */
            write_register(cpu, file, (uint8_t)(operand | mask));
        } else {
            bool bit_is_set = (operand & mask) != 0;
            bool should_skip = operation == 2 ? !bit_is_set : bit_is_set;
            if (should_skip) {                   /* BTFSC / BTFSS */
                cpu->pc = (uint16_t)((cpu->pc + 1u) & 0x01FFu);
                cycles = 2;
            }
        }
    } else if ((instruction & 0xF00u) == 0x800u) { /* RETLW k */
        cpu->w = (uint8_t)instruction;
        cpu->pc = pop_stack(cpu);
        cycles = 2;
    } else if ((instruction & 0xF00u) == 0x900u) { /* CALL k */
        push_stack(cpu, cpu->pc);
        /*
         * Baseline CALL 只携带 8 位目标；对 PIC10F200 来说目标位于
         * 当前 256 字页面内。blink.hex 正是用 CALL 0xFE 调跳板。
         */
        cpu->pc = (uint16_t)(instruction & 0x0FFu);
        cycles = 2;
    } else if ((instruction & 0xE00u) == 0xA00u) { /* GOTO k */
        cpu->pc = (uint16_t)(instruction & 0x1FFu);
        cycles = 2;
    } else if ((instruction & 0xF00u) == 0xC00u) { /* MOVLW k */
        cpu->w = (uint8_t)instruction;
    } else if ((instruction & 0xF00u) == 0xD00u) { /* IORLW k */
        cpu->w = (uint8_t)(cpu->w | (uint8_t)instruction);
        update_zero(cpu, cpu->w);
    } else if ((instruction & 0xF00u) == 0xE00u) { /* ANDLW k */
        cpu->w = (uint8_t)(cpu->w & (uint8_t)instruction);
        update_zero(cpu, cpu->w);
    } else if ((instruction & 0xF00u) == 0xF00u) { /* XORLW k */
        cpu->w = (uint8_t)(cpu->w ^ (uint8_t)instruction);
        update_zero(cpu, cpu->w);
    } else {
        cpu->stopped = true;
        cpu->stop_reason = "遇到未知或未实现的 12 位指令";
    }

    tick_timer0(cpu, cycles);
    cpu->cycles += cycles;
    result.reset_occurred = tick_watchdog(cpu, cycles) &&
        cpu->last_reset == PIC10_RESET_WATCHDOG;

    result.old_gpio = old_gpio;
    result.new_gpio = pic10f200_gpio_value(cpu);
    result.gpio_changed = result.old_gpio != result.new_gpio;
    result.instruction_cycles = cycles;
    return result;
}
