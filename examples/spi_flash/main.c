#include <xc.h>

#pragma config WDTE = OFF

/*
 * PIC10F202没有硬件SPI，本例用普通GPIO模拟SPI Mode 0。
 *
 * GP0：W25Q的/CS片选
 * GP1：W25Q的CLK时钟
 * GP2：W25Q的DI（MOSI），校验结束后同时用来驱动LED
 * GP3：W25Q的DO（MISO），只能作为输入
 */

#define PIN_CS   0x01
#define PIN_CLK  0x02
#define PIN_MOSI 0x04
#define FLASH_SELECT() do { \
    gpio_output &= (unsigned char)~PIN_CS; \
    GPIO = gpio_output; \
} while (0)
#define FLASH_DESELECT() do { \
    gpio_output |= PIN_CS; \
    GPIO = gpio_output; \
} while (0)

static unsigned char gpio_output = PIN_CS;
static unsigned char buffer[5];

static unsigned char spi_transfer(unsigned char value)
{
    unsigned char result = 0;
    unsigned char bit;

    for (bit = 0; bit < 8; ++bit) {
        if ((value & 0x80) != 0) {
            gpio_output |= PIN_MOSI;
        } else {
            gpio_output &= (unsigned char)~PIN_MOSI;
        }
        GPIO = gpio_output;

        gpio_output |= PIN_CLK;
        GPIO = gpio_output;
        result <<= 1;
        if (GP3 != 0) {
            result |= 1;
        }

        gpio_output &= (unsigned char)~PIN_CLK;
        GPIO = gpio_output;
        value <<= 1;
    }
    return result;
}

/*
 * 向指定地址写入一段数据，不局限于示例字符串。
 * PIC10F202的资源很小，本教学接口使用16位地址，并要求单次数据不跨256字节页。
 */
static void w25q_write(unsigned int address,
                       const unsigned char *data,
                       unsigned char length)
{
    unsigned char index;
    unsigned char status;

    FLASH_SELECT();
    spi_transfer(0x06);
    FLASH_DESELECT();

    FLASH_SELECT();
    spi_transfer(0x02);
    spi_transfer(0x00);
    spi_transfer((unsigned char)(address >> 8));
    spi_transfer((unsigned char)address);
    for (index = 0; index < length; ++index) {
        spi_transfer(data[index]);
    }
    FLASH_DESELECT();

    do {
        FLASH_SELECT();
        spi_transfer(0x05);
        status = spi_transfer(0x00);
        FLASH_DESELECT();
    } while ((status & 0x01) != 0);
}

/* 从指定地址连续读取任意内容到调用者提供的缓冲区。 */
static void w25q_read(unsigned int address,
                      unsigned char *data,
                      unsigned char length)
{
    unsigned char index;

    FLASH_SELECT();
    spi_transfer(0x03);
    spi_transfer(0x00);
    spi_transfer((unsigned char)(address >> 8));
    spi_transfer((unsigned char)address);
    for (index = 0; index < length; ++index) {
        data[index] = spi_transfer(0x00);
    }
    FLASH_DESELECT();
}

void main(void)
{
    unsigned char mismatch = 0;

    TRISGPIO = 0b111000; /* GP0、GP1、GP2输出，GP3输入 */
    GPIO = gpio_output;

    buffer[0] = 'H';
    buffer[1] = 'e';
    buffer[2] = 'l';
    buffer[3] = 'l';
    buffer[4] = 'o';
    w25q_write(0, buffer, sizeof(buffer));

    w25q_read(0, buffer, sizeof(buffer));
    mismatch |= buffer[0] ^ 'H';
    mismatch |= buffer[1] ^ 'e';
    mismatch |= buffer[2] ^ 'l';
    mismatch |= buffer[3] ^ 'l';
    mismatch |= buffer[4] ^ 'o';

    buffer[0] = 'W';
    buffer[1] = 'o';
    buffer[2] = 'r';
    buffer[3] = 'l';
    buffer[4] = 'd';
    w25q_write(5, buffer, sizeof(buffer));

    w25q_read(5, buffer, sizeof(buffer));
    mismatch |= buffer[0] ^ 'W';
    mismatch |= buffer[1] ^ 'o';
    mismatch |= buffer[2] ^ 'r';
    mismatch |= buffer[3] ^ 'l';
    mismatch |= buffer[4] ^ 'd';

    /* GP2亮表示写入和读回的数据完全一致。 */
    gpio_output = PIN_CS | (mismatch == 0 ? PIN_MOSI : 0);
    GPIO = gpio_output;

    for (;;) {
        NOP();
    }
}
