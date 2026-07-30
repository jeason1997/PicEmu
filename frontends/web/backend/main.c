#include "picemu/core/pic10_cpu.h"
#include "picemu/core/disassembler.h"
#include "picemu/firmware/hex_loader.h"
#include "picemu/sim/devices/w25q.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Web前端使用的本地后端进程。
 *
 * Node服务通过stdin发送制表符分隔的命令，本进程在stdout输出一行JSON。
 * 使用独立进程而不是复制JavaScript版CPU，确保CLI、SDL、STM32和Web共享
 * 完全相同的PIC指令实现。
 */
static Pic10Cpu cpu;
static HexImage image;
static bool loaded;
static uint64_t last_edge_cycle[4];
static double pin_frequency[4];
static double pin_duty[4];
static bool breakpoints[PIC10_MAX_PROGRAM_WORDS];
static bool breakpoint_hit;
static int resume_breakpoint = -1;
static SimW25q w25q;
static bool w25q_attached;
static unsigned w25q_cs_pin;
static unsigned w25q_clock_pin;
static unsigned w25q_mosi_pin;
static unsigned w25q_miso_pin;

static char *next_field(char **cursor);

static void print_json_string(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    putchar('"');
    while (*p != '\0') {
        if (*p == '"' || *p == '\\') {
            putchar('\\');
            putchar(*p);
        } else if (*p == '\n') {
            fputs("\\n", stdout);
        } else if (*p >= 0x20) {
            putchar(*p);
        }
        ++p;
    }
    putchar('"');
}

static void print_state(bool include_flash)
{
    unsigned i;

    printf("{\"ok\":true,\"loaded\":%s", loaded ? "true" : "false");
    if (!loaded) {
        puts("}");
        fflush(stdout);
        return;
    }

    printf(",\"device\":");
    print_json_string(cpu.device->name);
    printf(",\"cycles\":%" PRIu64
           ",\"pc\":%u,\"w\":%u,\"status\":%u,\"fsr\":%u"
           ",\"tmr0\":%u,\"osccal\":%u,\"gpio\":%u,\"gpioLatch\":%u"
           ",\"tris\":%u,\"option\":%u,\"config\":%u"
           ",\"sleeping\":%s,\"stopped\":%s",
           cpu.cycles, cpu.pc, cpu.w, cpu.ram[PIC10_STATUS],
           cpu.ram[PIC10_FSR], cpu.ram[PIC10_TMR0],
           cpu.ram[PIC10_OSCCAL], pic10_gpio_value(&cpu),
           cpu.gpio_latch, cpu.tris_gpio, cpu.option, cpu.config_word,
           cpu.sleeping ? "true" : "false",
           cpu.stopped ? "true" : "false");
    printf(",\"breakpointHit\":%s",
           breakpoint_hit ? "true" : "false");
    fputs(",\"pinFrequency\":[", stdout);
    for (i = 0; i < 4; ++i) {
        if (i != 0) putchar(',');
        printf("%.2f", pin_frequency[i]);
    }
    fputs("],\"pinDuty\":[", stdout);
    for (i = 0; i < 4; ++i) {
        if (i != 0) putchar(',');
        printf("%.4f", pin_duty[i]);
    }
    putchar(']');

    fputs(",\"stack\":[", stdout);
    printf("%u,%u],\"stackPointer\":%u,\"ram\":[",
           cpu.stack[0], cpu.stack[1], cpu.stack_pointer);
    for (i = 0; i < 32; ++i) {
        if (i != 0) putchar(',');
        printf("%u", cpu.ram[i]);
    }
    putchar(']');

    if (include_flash) {
        printf(",\"programWords\":%u,\"flash\":[",
               cpu.device->program_words);
        for (i = 0; i < cpu.device->program_words; ++i) {
            if (i != 0) putchar(',');
            printf("%u", cpu.program[i] & 0x0FFFu);
        }
        fputs("],\"instructions\":[", stdout);
        for (i = 0; i < cpu.device->program_words; ++i) {
            char assembly[64];
            if (i != 0) putchar(',');
            pic10_disassemble(cpu.program[i], assembly, sizeof(assembly));
            print_json_string(assembly);
        }
        putchar(']');
    }
    if (cpu.stop_reason != NULL) {
        fputs(",\"stopReason\":", stdout);
        print_json_string(cpu.stop_reason);
    }
    puts("}");
    fflush(stdout);
}

static void print_error(const char *message)
{
    fputs("{\"ok\":false,\"error\":", stdout);
    print_json_string(message);
    puts("}");
    fflush(stdout);
}

static void apply_inputs(unsigned mask, unsigned values)
{
    unsigned pin;
    for (pin = 0; pin < 4; ++pin) {
        unsigned bit = 1u << pin;
        pic10_drive_pin(&cpu, pin, (mask & bit) != 0,
                        (values & bit) != 0);
    }
}

static void drive_w25q_miso(void)
{
    if (w25q_attached) {
        pic10_drive_pin(&cpu, w25q_miso_pin, true,
                        sim_w25q_miso(&w25q));
    }
}

static void update_w25q_lines(void)
{
    uint8_t gpio;
    if (!w25q_attached) return;
    gpio = pic10_gpio_value(&cpu);
    sim_w25q_set_lines(
        &w25q,
        (gpio & (1u << w25q_cs_pin)) != 0,
        (gpio & (1u << w25q_clock_pin)) != 0,
        (gpio & (1u << w25q_mosi_pin)) != 0);
    drive_w25q_miso();
}

static unsigned step_cpu(void)
{
    unsigned consumed;
    drive_w25q_miso();
    consumed = pic10_step_cycles(&cpu);
    update_w25q_lines();
    return consumed;
}

static void execute_cycles(uint64_t requested)
{
    uint64_t target = cpu.cycles + requested;
    uint64_t high_cycles[4] = {0, 0, 0, 0};
    uint64_t measured_cycles = 0;
    bool skip_current =
        resume_breakpoint >= 0 && (unsigned)resume_breakpoint == cpu.pc;
    breakpoint_hit = false;
    resume_breakpoint = -1;
    while (!cpu.stopped && cpu.cycles < target) {
        uint8_t old_gpio = pic10_gpio_value(&cpu);
        uint64_t before = cpu.cycles;
        unsigned consumed;
        uint8_t changed;
        unsigned pin;
        if (cpu.pc < PIC10_MAX_PROGRAM_WORDS &&
            breakpoints[cpu.pc] && !skip_current) {
            breakpoint_hit = true;
            resume_breakpoint = (int)cpu.pc;
            break;
        }
        skip_current = false;
        consumed = step_cpu();
        changed = (uint8_t)(old_gpio ^ pic10_gpio_value(&cpu));
        if (consumed == 0) break;
        measured_cycles += consumed;
        for (pin = 0; pin < 4; ++pin) {
            if ((old_gpio & (1u << pin)) != 0) {
                high_cycles[pin] += consumed;
            }
            if ((changed & (1u << pin)) != 0) {
                uint64_t edge = before + consumed;
                uint64_t half_period = edge - last_edge_cycle[pin];
                if (last_edge_cycle[pin] != 0 && half_period != 0) {
                    pin_frequency[pin] = 500000.0 / (double)half_period;
                }
                last_edge_cycle[pin] = edge;
            }
        }
    }
    if (measured_cycles != 0) {
        unsigned pin;
        for (pin = 0; pin < 4; ++pin) {
            pin_duty[pin] =
                (double)high_cycles[pin] / (double)measured_cycles;
        }
    }
}

static bool parse_initial_data(const char *text, uint8_t *data,
                               size_t capacity, size_t *size)
{
    const char *cursor = text != NULL ? text : "";
    *size = 0;
    while (*cursor != '\0') {
        char *end;
        unsigned long value;
        while (*cursor == ' ' || *cursor == ',' || *cursor == ':') ++cursor;
        if (*cursor == '\0') break;
        value = strtoul(cursor, &end, 16);
        if (end == cursor || value > 0xFFu || *size >= capacity) return false;
        data[(*size)++] = (uint8_t)value;
        cursor = end;
    }
    return true;
}

static void configure_w25q(char **cursor)
{
    char *capacity_text = next_field(cursor);
    char *initial_text = next_field(cursor);
    char *cs_text = next_field(cursor);
    char *clock_text = next_field(cursor);
    char *mosi_text = next_field(cursor);
    char *miso_text = next_field(cursor);
    size_t capacity;
    uint8_t initial[512];
    size_t initial_size;
    unsigned pins[4];

    if (capacity_text == NULL || cs_text == NULL || clock_text == NULL ||
        mosi_text == NULL || miso_text == NULL) {
        print_error("W25Q配置参数不完整");
        return;
    }
    capacity = (size_t)strtoull(capacity_text, NULL, 0);
    pins[0] = (unsigned)strtoul(cs_text, NULL, 0);
    pins[1] = (unsigned)strtoul(clock_text, NULL, 0);
    pins[2] = (unsigned)strtoul(mosi_text, NULL, 0);
    pins[3] = (unsigned)strtoul(miso_text, NULL, 0);
    if (capacity < 128u * 1024u || capacity > 16u * 1024u * 1024u ||
        pins[0] > 3 || pins[1] > 3 || pins[2] > 3 || pins[3] > 3 ||
        !parse_initial_data(initial_text, initial, sizeof(initial),
                            &initial_size)) {
        print_error("无效的W25Q容量、引脚或初始化数据");
        return;
    }

    if (w25q_attached) sim_w25q_destroy(&w25q);
    if (!sim_w25q_init(&w25q, capacity, initial, initial_size)) {
        w25q_attached = false;
        print_error("无法分配W25Q存储空间");
        return;
    }
    w25q_cs_pin = pins[0];
    w25q_clock_pin = pins[1];
    w25q_mosi_pin = pins[2];
    w25q_miso_pin = pins[3];
    w25q_attached = true;
    update_w25q_lines();
    print_state(false);
}

static void print_w25q_data(char **cursor)
{
    char *offset_text = next_field(cursor);
    char *count_text = next_field(cursor);
    size_t offset;
    size_t count;
    size_t i;

    if (!w25q_attached || offset_text == NULL || count_text == NULL) {
        print_error("W25Q尚未配置");
        return;
    }
    offset = (size_t)strtoull(offset_text, NULL, 0);
    count = (size_t)strtoull(count_text, NULL, 0);
    if (count > 256 || offset > w25q.capacity ||
        count > w25q.capacity - offset) {
        print_error("W25Q读取范围无效");
        return;
    }
    printf("{\"ok\":true,\"capacity\":%zu,\"offset\":%zu,\"data\":[",
           w25q.capacity, offset);
    for (i = 0; i < count; ++i) {
        if (i != 0) putchar(',');
        printf("%u", w25q.data[offset + i]);
    }
    puts("]}");
    fflush(stdout);
}

static void set_breakpoints(const char *list)
{
    const char *cursor = list;
    memset(breakpoints, 0, sizeof(breakpoints));
    while (cursor != NULL && *cursor != '\0') {
        char *end;
        unsigned long address = strtoul(cursor, &end, 0);
        if (end == cursor) break;
        if (address < PIC10_MAX_PROGRAM_WORDS) {
            breakpoints[address] = true;
        }
        cursor = *end == ',' ? end + 1 : NULL;
    }
    breakpoint_hit = false;
    resume_breakpoint = -1;
}

static char *next_field(char **cursor)
{
    char *start = *cursor;
    char *tab;
    if (start == NULL) return NULL;
    tab = strchr(start, '\t');
    if (tab != NULL) {
        *tab = '\0';
        *cursor = tab + 1;
    } else {
        *cursor = NULL;
    }
    return start;
}

int main(void)
{
    char line[2048];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *cursor = line;
        char *command;
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }
        command = next_field(&cursor);

        if (strcmp(command, "load") == 0) {
            char *path = next_field(&cursor);
            char *device_name = next_field(&cursor);
            const PicDeviceDescription *device =
                pic_device_find(device_name != NULL
                                ? device_name : "PIC10F200");
            char error[256];
            if (path == NULL || device == NULL) {
                print_error("无效的固件路径或PIC型号");
            } else if (!hex_load_file(path, &image, error, sizeof(error))) {
                print_error(error);
            } else {
                pic10_init(&cpu, &image, device);
                memset(last_edge_cycle, 0, sizeof(last_edge_cycle));
                memset(pin_frequency, 0, sizeof(pin_frequency));
                memset(pin_duty, 0, sizeof(pin_duty));
                memset(breakpoints, 0, sizeof(breakpoints));
                breakpoint_hit = false;
                resume_breakpoint = -1;
                if (w25q_attached) {
                    sim_w25q_destroy(&w25q);
                    w25q_attached = false;
                }
                loaded = true;
                print_state(true);
            }
        } else if (!loaded) {
            print_error("尚未加载PIC固件");
        } else if (strcmp(command, "reset") == 0) {
            pic10_reset(&cpu, PIC10_RESET_MCLR);
            memset(last_edge_cycle, 0, sizeof(last_edge_cycle));
            memset(pin_frequency, 0, sizeof(pin_frequency));
            memset(pin_duty, 0, sizeof(pin_duty));
            breakpoint_hit = false;
            resume_breakpoint = -1;
            if (w25q_attached) {
                sim_w25q_reset_bus(&w25q);
                update_w25q_lines();
            }
            print_state(false);
        } else if (strcmp(command, "step") == 0) {
            unsigned mask = (unsigned)strtoul(
                next_field(&cursor), NULL, 0);
            unsigned values = (unsigned)strtoul(
                next_field(&cursor), NULL, 0);
            apply_inputs(mask, values);
            breakpoint_hit = false;
            resume_breakpoint = -1;
            step_cpu();
            print_state(false);
        } else if (strcmp(command, "run") == 0) {
            uint64_t cycles = strtoull(next_field(&cursor), NULL, 0);
            unsigned mask = (unsigned)strtoul(
                next_field(&cursor), NULL, 0);
            unsigned values = (unsigned)strtoul(
                next_field(&cursor), NULL, 0);
            apply_inputs(mask, values);
            execute_cycles(cycles);
            print_state(false);
        } else if (strcmp(command, "state") == 0) {
            print_state(false);
        } else if (strcmp(command, "flash") == 0) {
            print_state(true);
        } else if (strcmp(command, "breakpoints") == 0) {
            set_breakpoints(next_field(&cursor));
            print_state(false);
        } else if (strcmp(command, "w25_config") == 0) {
            configure_w25q(&cursor);
        } else if (strcmp(command, "w25_read") == 0) {
            print_w25q_data(&cursor);
        } else {
            print_error("未知的Web后端命令");
        }
    }
    return 0;
}
