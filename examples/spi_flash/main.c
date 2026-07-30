#include <xc.h>

#pragma config WDTE = OFF

/*
 * PIC10F200没有硬件SPI，本例用普通GPIO模拟SPI Mode 0。
 *
 * GP0：W25Q的/CS片选
 * GP1：W25Q的CLK时钟
 * GP2：W25Q的DI（MOSI），校验结束后同时用来驱动LED
 * GP3：W25Q的DO（MISO），只能作为输入
 */

#define PIN_CS   0x01
#define PIN_CLK  0x02
#define PIN_MOSI 0x04

static unsigned char gpio_output = PIN_CS;

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

static unsigned char flash_read_byte(unsigned char address)
{
    unsigned char value;

    gpio_output &= (unsigned char)~PIN_CS;
    GPIO = gpio_output;
    spi_transfer(0x03); /* 低速读取命令 */
    spi_transfer(0x00); /* 24位地址的高字节 */
    spi_transfer(0x00);
    spi_transfer(address);
    value = spi_transfer(0x00);
    gpio_output |= PIN_CS;
    GPIO = gpio_output;
    return value;
}

void main(void)
{
    unsigned char ok;

    TRISGPIO = 0b111000; /* GP0、GP1、GP2输出，GP3输入 */
    GPIO = gpio_output;

    /* 检查Flash地址0开始的“PICEMU!”七个字符。 */
    ok = flash_read_byte(0) == 'P';
    ok &= flash_read_byte(1) == 'I';
    ok &= flash_read_byte(2) == 'C';
    ok &= flash_read_byte(3) == 'E';
    ok &= flash_read_byte(4) == 'M';
    ok &= flash_read_byte(5) == 'U';
    ok &= flash_read_byte(6) == '!';

    /* 结束SPI通信后，GP2复用为结果LED：亮表示读取和校验成功。 */
    gpio_output = PIN_CS | (ok ? PIN_MOSI : 0);
    GPIO = gpio_output;

    for (;;) {
        NOP();
    }
}
