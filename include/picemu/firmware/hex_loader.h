#ifndef HEX_LOADER_H
#define HEX_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * 当前支持的PIC10 Baseline器件最多有512个12位程序字。
 * Intel HEX 按“字节地址”保存数据，因此一个程序字在 HEX 中占两个字节。
 */
#define PIC10_MAX_PROGRAM_WORDS 512u

typedef struct {
    uint16_t program[PIC10_MAX_PROGRAM_WORDS];
    bool program_present[PIC10_MAX_PROGRAM_WORDS];

    /* PIC10F200 的配置字在 MPLAB HEX 中通常位于字节地址 0x1FFE。 */
    uint16_t config_word;
    bool config_present;
} HexImage;

/*
 * 加载并校验 Intel HEX 文件。
 *
 * 成功返回 true；失败返回 false，并把可读的错误信息写入 error。
 * 对不属于程序区和配置字的记录会安全忽略，这使加载器可以接受
 * MPLAB 生成的、还包含器件 ID 等额外区域的 HEX 文件。
 */
bool hex_load_file(const char *path, HexImage *image,
                   char *error, size_t error_size);

#endif
