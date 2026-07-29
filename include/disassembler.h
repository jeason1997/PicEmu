#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

/*
 * 把一个 PIC10F200 12 位机器指令转换成人类可读的汇编文本。
 * 返回 true 表示指令编码已识别；false 表示该编码未定义。
 */
int pic10_disassemble(uint16_t instruction, char *output, size_t output_size);

/* 返回特殊功能寄存器名称；普通 RAM 地址返回 NULL。 */
const char *pic10_register_name(uint8_t address);

#endif
