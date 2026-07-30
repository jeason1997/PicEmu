#include "picemu/sim/devices/w25q.h"

#include <stdlib.h>
#include <string.h>

enum {
    W25Q_COMMAND_PAGE_PROGRAM = 0x02,
    W25Q_COMMAND_READ = 0x03,
    W25Q_COMMAND_READ_STATUS = 0x05,
    W25Q_COMMAND_WRITE_ENABLE = 0x06,
    W25Q_COMMAND_JEDEC_ID = 0x9F
};

static void reset_transaction(SimW25q *flash)
{
    flash->command = 0;
    flash->input_byte = 0;
    flash->input_bits = 0;
    flash->address = 0;
    flash->address_bytes = 0;
    flash->output_active = false;
    flash->hold_first_falling = false;
    flash->output_byte = 0;
    flash->output_bit = 0;
    flash->jedec_index = 0;
    flash->miso_high = false;
}

static uint8_t jedec_byte(const SimW25q *flash, unsigned index)
{
    size_t value;
    uint8_t exponent = 0;

    if (index == 0) return 0xEF; /* Winbond厂商编号 */
    if (index == 1) return 0x40; /* W25Q系列存储器类型 */
    value = flash->capacity;
    while (value > 1) {
        value >>= 1;
        ++exponent;
    }
    return exponent;
}

static void begin_output(SimW25q *flash, uint8_t value)
{
    flash->output_active = true;
    flash->hold_first_falling = true;
    flash->output_byte = value;
    flash->output_bit = 7;
    flash->miso_high = (value & 0x80u) != 0;
}

static uint8_t read_current_byte(const SimW25q *flash)
{
    return flash->capacity == 0
        ? 0xFFu : flash->data[flash->address % flash->capacity];
}

static void accept_byte(SimW25q *flash, uint8_t value)
{
    if (flash->command == 0) {
        flash->command = value;
        if (value == W25Q_COMMAND_JEDEC_ID) {
            flash->jedec_index = 0;
            begin_output(flash, jedec_byte(flash, 0));
        } else if (value == W25Q_COMMAND_READ_STATUS) {
            begin_output(flash, flash->write_enable ? 0x02u : 0x00u);
        } else if (value == W25Q_COMMAND_WRITE_ENABLE) {
            flash->write_enable = true;
        }
        return;
    }

    if ((flash->command == W25Q_COMMAND_READ ||
         flash->command == W25Q_COMMAND_PAGE_PROGRAM) &&
        flash->address_bytes < 3) {
        flash->address = (flash->address << 8) | value;
        ++flash->address_bytes;
        if (flash->address_bytes == 3 &&
            flash->command == W25Q_COMMAND_READ) {
            begin_output(flash, read_current_byte(flash));
        }
        return;
    }

    if (flash->command == W25Q_COMMAND_PAGE_PROGRAM &&
        flash->address_bytes == 3 && flash->write_enable) {
        uint32_t page_base = flash->address & ~0xFFu;
        uint32_t next_offset = (flash->address + 1u) & 0xFFu;
        size_t index = flash->address % flash->capacity;
        /* NOR Flash页编程只能把位从1写成0，擦除后才能重新变成1。 */
        flash->data[index] &= value;
        flash->address = page_base | next_offset;
    }
}

static void advance_output(SimW25q *flash)
{
    if (!flash->output_active) return;
    if (flash->hold_first_falling) {
        flash->hold_first_falling = false;
        return;
    }
    if (flash->output_bit > 0) {
        --flash->output_bit;
        flash->miso_high =
            (flash->output_byte & (1u << flash->output_bit)) != 0;
        return;
    }

    if (flash->command == W25Q_COMMAND_READ) {
        flash->address = (flash->address + 1u) & 0x00FFFFFFu;
        begin_output(flash, read_current_byte(flash));
        flash->hold_first_falling = false;
    } else if (flash->command == W25Q_COMMAND_JEDEC_ID) {
        flash->jedec_index =
            (flash->jedec_index + 1u) % 3u;
        begin_output(flash, jedec_byte(flash, flash->jedec_index));
        flash->hold_first_falling = false;
    } else if (flash->command == W25Q_COMMAND_READ_STATUS) {
        begin_output(flash, flash->write_enable ? 0x02u : 0x00u);
        flash->hold_first_falling = false;
    } else {
        flash->output_active = false;
        flash->miso_high = false;
    }
}

bool sim_w25q_init(SimW25q *flash, size_t capacity,
                   const uint8_t *initial_data, size_t initial_size)
{
    if (flash == NULL || capacity == 0) return false;
    memset(flash, 0, sizeof(*flash));
    flash->data = malloc(capacity);
    if (flash->data == NULL) return false;
    flash->capacity = capacity;
    memset(flash->data, 0xFF, capacity);
    if (initial_data != NULL && initial_size != 0) {
        if (initial_size > capacity) initial_size = capacity;
        memcpy(flash->data, initial_data, initial_size);
    }
    sim_w25q_reset_bus(flash);
    return true;
}

void sim_w25q_destroy(SimW25q *flash)
{
    if (flash == NULL) return;
    free(flash->data);
    memset(flash, 0, sizeof(*flash));
}

void sim_w25q_reset_bus(SimW25q *flash)
{
    if (flash == NULL) return;
    flash->cs_high = true;
    flash->clock_high = false;
    flash->mosi_high = false;
    flash->miso_high = false;
    flash->write_enable = false;
    reset_transaction(flash);
}

void sim_w25q_set_lines(SimW25q *flash, bool cs_high,
                        bool clock_high, bool mosi_high)
{
    bool rising;
    bool falling;

    if (flash == NULL || flash->data == NULL) return;
    if (cs_high) {
        if (!flash->cs_high) {
            if (flash->command == W25Q_COMMAND_PAGE_PROGRAM) {
                flash->write_enable = false;
            }
            reset_transaction(flash);
        }
        flash->cs_high = true;
        flash->clock_high = clock_high;
        flash->mosi_high = mosi_high;
        return;
    }
    if (flash->cs_high) {
        flash->cs_high = false;
        flash->clock_high = false;
        reset_transaction(flash);
    }

    rising = !flash->clock_high && clock_high;
    falling = flash->clock_high && !clock_high;
    flash->mosi_high = mosi_high;

    if (rising && !flash->output_active) {
        flash->input_byte =
            (uint8_t)((flash->input_byte << 1) | (mosi_high ? 1u : 0u));
        if (++flash->input_bits == 8) {
            uint8_t value = flash->input_byte;
            flash->input_byte = 0;
            flash->input_bits = 0;
            accept_byte(flash, value);
        }
    }
    if (falling) advance_output(flash);
    flash->clock_high = clock_high;
}

bool sim_w25q_miso(const SimW25q *flash)
{
    return flash != NULL && !flash->cs_high && flash->miso_high;
}

bool sim_w25q_read(const SimW25q *flash, size_t offset,
                   uint8_t *target, size_t count)
{
    if (flash == NULL || flash->data == NULL || target == NULL ||
        offset > flash->capacity || count > flash->capacity - offset) {
        return false;
    }
    memcpy(target, flash->data + offset, count);
    return true;
}

bool sim_w25q_write(SimW25q *flash, size_t offset,
                    const uint8_t *source, size_t count)
{
    if (flash == NULL || flash->data == NULL || source == NULL ||
        offset > flash->capacity || count > flash->capacity - offset) {
        return false;
    }
    /*
     * 这是供仿真器属性面板使用的数据编辑接口，不是SPI页编程命令，
     * 因此允许直接设置0和1。固件通过SPI写入时仍遵守NOR只能1变0的约束。
     */
    memcpy(flash->data + offset, source, count);
    return true;
}
