#include "picemu/firmware/hex_loader.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEX_LINE_CAPACITY 1024
#define PIC10_CONFIG_BYTE_ADDRESS 0x1FFEu

static int hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static bool parse_hex_byte(const char *text, uint8_t *value)
{
    int high = hex_digit(text[0]);
    int low = hex_digit(text[1]);

    if (high < 0 || low < 0) {
        return false;
    }
    *value = (uint8_t)((high << 4) | low);
    return true;
}

static void set_error(char *error, size_t size, const char *message,
                      unsigned line_number)
{
    if (size == 0) {
        return;
    }
    if (line_number == 0) {
        snprintf(error, size, "%s", message);
    } else {
        snprintf(error, size, "HEX 第 %u 行：%s", line_number, message);
    }
}

bool hex_load_file(const char *path, HexImage *image,
                   char *error, size_t error_size)
{
    FILE *file;
    char line[HEX_LINE_CAPACITY];
    unsigned line_number = 0;
    uint32_t address_base = 0;
    uint8_t program_bytes[PIC10F200_PROGRAM_WORDS * 2] = {0};
    bool byte_present[PIC10F200_PROGRAM_WORDS * 2] = {false};
    uint8_t config_bytes[2] = {0};
    bool config_byte_present[2] = {false};
    bool saw_eof = false;

    if (path == NULL || image == NULL) {
        set_error(error, error_size, "传入了空指针", 0);
        return false;
    }

    memset(image, 0, sizeof(*image));
    file = fopen(path, "r");
    if (file == NULL) {
        if (error_size > 0) {
            snprintf(error, error_size, "无法打开 %s：%s", path,
                     strerror(errno));
        }
        return false;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t record[HEX_LINE_CAPACITY / 2];
        size_t length;
        size_t record_size;
        size_t i;
        unsigned sum = 0;
        uint8_t byte_count;
        uint16_t offset;
        uint8_t record_type;

        ++line_number;
        length = strcspn(line, "\r\n");
        line[length] = '\0';

        if (length == 0) {
            continue;
        }
        if (line[0] != ':') {
            set_error(error, error_size, "记录没有以冒号开头", line_number);
            fclose(file);
            return false;
        }
        if (((length - 1) & 1u) != 0 || length < 11) {
            set_error(error, error_size, "记录长度不合法", line_number);
            fclose(file);
            return false;
        }

        record_size = (length - 1) / 2;
        for (i = 0; i < record_size; ++i) {
            if (!parse_hex_byte(&line[1 + i * 2], &record[i])) {
                set_error(error, error_size, "包含非十六进制字符",
                          line_number);
                fclose(file);
                return false;
            }
            sum += record[i];
        }

        byte_count = record[0];
        if (record_size != (size_t)byte_count + 5u) {
            set_error(error, error_size, "字节数量字段与记录长度不一致",
                      line_number);
            fclose(file);
            return false;
        }
        if ((sum & 0xFFu) != 0) {
            set_error(error, error_size, "校验和错误", line_number);
            fclose(file);
            return false;
        }

        offset = (uint16_t)(((uint16_t)record[1] << 8) | record[2]);
        record_type = record[3];

        if (record_type == 0x00) {
            for (i = 0; i < byte_count; ++i) {
                uint32_t absolute = address_base + offset + (uint32_t)i;
                uint8_t value = record[4 + i];

                if (absolute < sizeof(program_bytes)) {
                    program_bytes[absolute] = value;
                    byte_present[absolute] = true;
                } else if (absolute >= PIC10_CONFIG_BYTE_ADDRESS &&
                           absolute < PIC10_CONFIG_BYTE_ADDRESS + 2u) {
                    unsigned index =
                        (unsigned)(absolute - PIC10_CONFIG_BYTE_ADDRESS);
                    config_bytes[index] = value;
                    config_byte_present[index] = true;
                }
            }
        } else if (record_type == 0x01) {
            saw_eof = true;
            break;
        } else if (record_type == 0x02 && byte_count == 2) {
            uint16_t segment =
                (uint16_t)(((uint16_t)record[4] << 8) | record[5]);
            address_base = (uint32_t)segment << 4;
        } else if (record_type == 0x04 && byte_count == 2) {
            uint16_t upper =
                (uint16_t)(((uint16_t)record[4] << 8) | record[5]);
            address_base = (uint32_t)upper << 16;
        }
        /* 03、05 是启动地址记录，对 PIC10F200 的复位入口没有作用。 */
    }

    if (ferror(file)) {
        set_error(error, error_size, "读取文件时发生 I/O 错误", line_number);
        fclose(file);
        return false;
    }
    fclose(file);

    if (!saw_eof) {
        set_error(error, error_size, "缺少 EOF 记录", line_number);
        return false;
    }

    for (size_t word = 0; word < PIC10F200_PROGRAM_WORDS; ++word) {
        size_t low = word * 2;
        size_t high = low + 1;

        /* 未写入的 Flash 在真实器件上读作全 1，即 0xFFF。 */
        image->program[word] = 0x0FFFu;
        if (byte_present[low] || byte_present[high]) {
            image->program[word] =
                (uint16_t)((program_bytes[low] |
                           ((uint16_t)program_bytes[high] << 8)) & 0x0FFFu);
            image->program_present[word] = true;
        }
    }

    if (config_byte_present[0] || config_byte_present[1]) {
        image->config_word =
            (uint16_t)((config_bytes[0] |
                       ((uint16_t)config_bytes[1] << 8)) & 0x0FFFu);
        image->config_present = true;
    }

    return true;
}
