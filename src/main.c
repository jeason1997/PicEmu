#include "hex_loader.h"
#include "pic10f200.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
    printf("用法：%s firmware.hex [--cycles 数量] [--quiet]\n", program);
    printf("\n");
    printf("  --cycles N  最多执行 N 个指令周期，默认 1000000\n");
    printf("  --quiet     不逐条打印 GPIO 变化，只显示最终摘要\n");
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

int main(int argc, char **argv)
{
    const char *hex_path;
    uint64_t cycle_limit = 1000000;
    bool quiet = false;
    HexImage image;
    Pic10F200 cpu;
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
            if (i + 1 >= argc || !parse_u64(argv[++i], &cycle_limit)) {
                fprintf(stderr, "--cycles 后需要一个非负整数\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
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
        if (image.program_present[i]) {
            ++loaded_words;
        }
    }

    printf("已加载：%s\n", hex_path);
    printf("程序字：%u / %u", loaded_words, PIC10F200_PROGRAM_WORDS);
    if (image.config_present) {
        printf("，配置字：0x%03X", image.config_word);
    }
    printf("\n");

    pic10f200_init(&cpu, &image);

    while (cpu.cycles < cycle_limit && !cpu.stopped && !cpu.sleeping) {
        Pic10StepResult step = pic10f200_step(&cpu);

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
    }

    printf("执行结束：周期=%" PRIu64 "，PC=0x%03X，W=0x%02X，"
           "GPIO=0x%X，GPIO变化=%u 次\n",
           cpu.cycles, cpu.pc, cpu.w, pic10f200_gpio_value(&cpu),
           gpio_events);

    if (cpu.stopped) {
        fprintf(stderr, "模拟器停止：%s\n",
                cpu.stop_reason != NULL ? cpu.stop_reason : "未知原因");
        return EXIT_FAILURE;
    }
    if (cpu.sleeping) {
        printf("CPU 已执行 SLEEP；当前未注入唤醒事件。\n");
    } else if (cpu.cycles >= cycle_limit) {
        printf("已达到 --cycles 限制。\n");
    }

    return EXIT_SUCCESS;
}
