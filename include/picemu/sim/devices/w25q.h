#ifndef PICEMU_SIM_DEVICES_W25Q_H
#define PICEMU_SIM_DEVICES_W25Q_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * W25Q系列SPI NOR Flash的数字协议模型。
 *
 * 当前实现覆盖教学示例最常用的基本读写命令：
 *   0x02：页编程
 *   0x03：按24位地址连续读取
 *   0x05：读取状态寄存器
 *   0x06：写使能
 *   0x9F：读取JEDEC ID
 *
 * 模型工作在SPI Mode 0：上升沿采样MOSI，下降沿更新MISO。
 */
typedef struct {
    uint8_t *data;
    size_t capacity;

    bool cs_high;
    bool clock_high;
    bool mosi_high;
    bool miso_high;

    uint8_t command;
    uint8_t input_byte;
    unsigned input_bits;
    uint32_t address;
    unsigned address_bytes;

    bool output_active;
    bool hold_first_falling;
    bool write_enable;
    uint8_t output_byte;
    unsigned output_bit;
    unsigned jedec_index;
} SimW25q;

bool sim_w25q_init(SimW25q *flash, size_t capacity,
                   const uint8_t *initial_data, size_t initial_size);
void sim_w25q_destroy(SimW25q *flash);
void sim_w25q_reset_bus(SimW25q *flash);
void sim_w25q_set_lines(SimW25q *flash, bool cs_high,
                        bool clock_high, bool mosi_high);
bool sim_w25q_miso(const SimW25q *flash);
bool sim_w25q_read(const SimW25q *flash, size_t offset,
                   uint8_t *target, size_t count);
bool sim_w25q_write(SimW25q *flash, size_t offset,
                    const uint8_t *source, size_t count);

#endif
