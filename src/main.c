#include "disassembler.h"
#include "hex_loader.h"
#include "pic10f200.h"
#include "vcd_writer.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PIN_EVENTS 1024

typedef struct {
    uint64_t cycle;
    unsigned pin;
    bool driven;
    bool high;
} PinEvent;

static void print_usage(const char *program)
{
    printf("用法：%s firmware.hex [选项]\n\n", program);
    printf("  --cycles N          最多模拟N个指令周期，默认1000000\n");
    printf("  --quiet             不打印GPIO变化\n");
    printf("  --trace             打印每条执行指令及执行前状态\n");
    printf("  --disassemble       反汇编HEX中的程序并退出\n");
    printf("  --break ADDRESS     执行到指定PC前停止，可使用0x前缀\n");
    printf("  --dump              结束时显示寄存器、硬件栈和RAM\n");
    printf("  --vcd FILE          生成可由GTKWave打开的VCD波形\n");
    printf("  --events FILE       按周期向外部GPIO注入事件\n");
    printf("  -h, --help          显示帮助\n\n");
    printf("事件文件每行格式：cycle pin value，例如：1000 GP3 0\n");
    printf("value可为0、1或z；z表示外部电路释放引脚。\n");
}

static bool parse_u64(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_u16(const char *text, uint16_t *value)
{
    uint64_t parsed;

    if (!parse_u64(text, &parsed) || parsed > 0xFFFFu) {
        return false;
    }
    *value = (uint16_t)parsed;
    return true;
}

static int compare_events(const void *left, const void *right)
{
    const PinEvent *a = left;
    const PinEvent *b = right;
    return a->cycle < b->cycle ? -1 : a->cycle > b->cycle;
}

static bool load_events(const char *path, PinEvent *events, size_t *count,
                        char *error, size_t error_size)
{
    FILE *file = fopen(path, "r");
    char line[256];
    unsigned line_number = 0;

    if (file == NULL) {
        snprintf(error, error_size, "无法打开事件文件%s：%s",
                 path, strerror(errno));
        return false;
    }

    *count = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned long long cycle;
        char pin_text[16];
        char value_text[16];
        char extra[2];
        char *start = line;
        unsigned pin;
        PinEvent event;

        ++line_number;
        while (*start == ' ' || *start == '\t') {
            ++start;
        }
        if (*start == '\0' || *start == '\n' || *start == '#') {
            continue;
        }
        if (sscanf(start, "%llu %15s %15s %1s",
                   &cycle, pin_text, value_text, extra) != 3) {
            snprintf(error, error_size, "事件文件第%u行格式错误",
                     line_number);
            fclose(file);
            return false;
        }
        if ((pin_text[0] == 'G' || pin_text[0] == 'g') &&
            (pin_text[1] == 'P' || pin_text[1] == 'p')) {
            pin = (unsigned)strtoul(pin_text + 2, NULL, 10);
        } else {
            pin = (unsigned)strtoul(pin_text, NULL, 10);
        }
        if (pin >= 4 || *count >= MAX_PIN_EVENTS) {
            snprintf(error, error_size,
                     "事件文件第%u行引脚无效或事件过多", line_number);
            fclose(file);
            return false;
        }

        event.cycle = (uint64_t)cycle;
        event.pin = pin;
        event.driven = value_text[0] != 'z' && value_text[0] != 'Z';
        event.high = value_text[0] == '1';
        if (event.driven && value_text[0] != '0' && value_text[0] != '1') {
            snprintf(error, error_size, "事件文件第%u行电平必须为0、1或z",
                     line_number);
            fclose(file);
            return false;
        }
        events[(*count)++] = event;
    }

    fclose(file);
    qsort(events, *count, sizeof(events[0]), compare_events);
    return true;
}

static void dump_state(Pic10F200 *cpu)
{
    unsigned address;

    printf("\n寄存器状态：\n");
    printf("  PC=0x%03X  W=0x%02X  STATUS=0x%02X  FSR=0x%02X\n",
           cpu->pc, cpu->w, cpu->ram[PIC10_STATUS], cpu->ram[PIC10_FSR]);
    printf("  TMR0=0x%02X  OPTION=0x%02X  GPIO=0x%X  TRIS=0x%X\n",
           cpu->ram[PIC10_TMR0], cpu->option,
           pic10f200_gpio_value(cpu), cpu->tris_gpio);
    printf("  STACK=[0x%03X,0x%03X]  SP=%u  WDT=%u  SLEEP=%s\n",
           cpu->stack[0], cpu->stack[1], cpu->stack_pointer,
           cpu->watchdog_counter, cpu->sleeping ? "是" : "否");
    printf("通用RAM：\n");
    for (address = 0x10; address <= 0x1F; ++address) {
        printf("  %02X:%02X%s", address, cpu->ram[address],
               ((address - 0x0F) % 8) == 0 ? "\n" : " ");
    }
}

static void disassemble_image(const HexImage *image)
{
    unsigned address;

    for (address = 0; address < PIC10F200_PROGRAM_WORDS; ++address) {
        if (image->program_present[address]) {
            char text[64];
            pic10_disassemble(image->program[address], text, sizeof(text));
            printf("%03X: %03X  %s\n", address,
                   image->program[address], text);
        }
    }
}

int main(int argc, char **argv)
{
    const char *hex_path;
    const char *vcd_path = NULL;
    const char *events_path = NULL;
    uint64_t cycle_limit = 1000000;
    uint16_t breakpoint = 0;
    bool has_breakpoint = false;
    bool quiet = false;
    bool trace = false;
    bool disassemble = false;
    bool dump = false;
    HexImage image;
    Pic10F200 cpu;
    VcdWriter vcd = {0};
    PinEvent events[MAX_PIN_EVENTS];
    size_t event_count = 0;
    size_t next_event = 0;
    char error[256];
    unsigned loaded_words = 0;
    unsigned gpio_events = 0;
    int i;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    hex_path = argv[1];

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--cycles") == 0) {
            if (++i >= argc || !parse_u64(argv[i], &cycle_limit)) {
                fprintf(stderr, "--cycles后需要一个非负整数\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--break") == 0) {
            if (++i >= argc || !parse_u16(argv[i], &breakpoint)) {
                fprintf(stderr, "--break后需要一个程序地址\n");
                return EXIT_FAILURE;
            }
            has_breakpoint = true;
        } else if (strcmp(argv[i], "--vcd") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--vcd后需要输出文件名\n");
                return EXIT_FAILURE;
            }
            vcd_path = argv[i];
        } else if (strcmp(argv[i], "--events") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--events后需要事件文件名\n");
                return EXIT_FAILURE;
            }
            events_path = argv[i];
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = true;
        } else if (strcmp(argv[i], "--disassemble") == 0) {
            disassemble = true;
        } else if (strcmp(argv[i], "--dump") == 0) {
            dump = true;
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "未知参数：%s\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!hex_load_file(hex_path, &image, error, sizeof(error))) {
        fprintf(stderr, "加载失败：%s\n", error);
        return EXIT_FAILURE;
    }
    for (i = 0; i < (int)PIC10F200_PROGRAM_WORDS; ++i) {
        loaded_words += image.program_present[i] ? 1u : 0u;
    }

    printf("已加载：%s\n", hex_path);
    printf("程序字：%u / %u", loaded_words, PIC10F200_PROGRAM_WORDS);
    if (image.config_present) {
        printf("，配置字：0x%03X", image.config_word);
    }
    printf("\n");

    if (disassemble) {
        disassemble_image(&image);
        return EXIT_SUCCESS;
    }
    if (events_path != NULL &&
        !load_events(events_path, events, &event_count,
                     error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }

    pic10f200_init(&cpu, &image);
    if (vcd_path != NULL && !vcd_open(&vcd, vcd_path, &cpu)) {
        fprintf(stderr, "无法创建VCD文件：%s\n", vcd_path);
        return EXIT_FAILURE;
    }

    while (cpu.cycles < cycle_limit && !cpu.stopped) {
        Pic10StepResult step;
        uint16_t pc_before;
        uint16_t instruction;

        while (next_event < event_count &&
               events[next_event].cycle <= cpu.cycles) {
            PinEvent *event = &events[next_event++];
            pic10f200_drive_pin(&cpu, event->pin,
                                event->driven, event->high);
        }
        if (has_breakpoint && !cpu.sleeping && cpu.pc == breakpoint) {
            printf("命中断点：PC=0x%03X，周期=%" PRIu64 "\n",
                   cpu.pc, cpu.cycles);
            break;
        }

        pc_before = cpu.pc;
        instruction = cpu.sleeping ? 0 : cpu.program[cpu.pc];
        if (trace && !cpu.sleeping) {
            char assembly[64];
            pic10_disassemble(instruction, assembly, sizeof(assembly));
            printf("[周期 %10" PRIu64 "] PC=%03X OP=%03X %-18s "
                   "W=%02X STATUS=%02X GPIO=%X\n",
                   cpu.cycles, pc_before, instruction, assembly,
                   cpu.w, cpu.ram[PIC10_STATUS],
                   pic10f200_gpio_value(&cpu));
        }

        step = pic10f200_step(&cpu);
        vcd_sample(&vcd, &cpu);

        if (step.gpio_changed) {
            uint8_t changed = (uint8_t)(step.old_gpio ^ step.new_gpio);
            unsigned pin;

            for (pin = 0; pin < 4; ++pin) {
                if ((changed & (1u << pin)) != 0) {
                    ++gpio_events;
                    if (!quiet) {
                        printf("[周期 %10" PRIu64 "] GP%u -> %u\n",
                               cpu.cycles, pin,
                               (step.new_gpio >> pin) & 1u);
                    }
                }
            }
        }
        if (step.reset_occurred && !quiet) {
            printf("[周期 %10" PRIu64 "] WDT复位\n", cpu.cycles);
        } else if (step.woke_from_sleep && !quiet) {
            printf("[周期 %10" PRIu64 "] WDT从Sleep唤醒\n", cpu.cycles);
        }
    }

    vcd_close(&vcd);
    printf("执行结束：周期=%" PRIu64 "，PC=0x%03X，W=0x%02X，"
           "GPIO=0x%X，GPIO变化=%u次\n",
           cpu.cycles, cpu.pc, cpu.w, pic10f200_gpio_value(&cpu),
           gpio_events);
    if (dump) {
        dump_state(&cpu);
    }
    if (cpu.stopped) {
        fprintf(stderr, "模拟器停止：%s\n",
                cpu.stop_reason != NULL ? cpu.stop_reason : "未知原因");
        return EXIT_FAILURE;
    }
    if (cpu.cycles >= cycle_limit) {
        printf("已达到--cycles限制。\n");
    }
    return EXIT_SUCCESS;
}
