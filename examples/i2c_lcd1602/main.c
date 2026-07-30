#include <xc.h>

#pragma config WDTE = OFF

/* GP0连接SDA，GP1连接SCL；Web器件模拟地址为0x27的PCF8574。 */
#define SDA 0x01u
#define SCL 0x02u

static unsigned char pins = SDA | SCL;
#define lines(value) do { pins = (value); GPIO = pins; NOP(); } while (0)

static void i2c_write(unsigned char value)
{
    unsigned char bits = 8;
    do {
        lines((pins & (unsigned char)~(SDA | SCL)) |
              ((value & 0x80u) ? SDA : 0));
        lines(pins | SCL);
        lines(pins & (unsigned char)~SCL);
        value <<= 1;
    } while (--bits);
    lines(pins | SDA);
    lines(pins | SCL);
    lines(pins & (unsigned char)~SCL);
}

static void expander(unsigned char value)
{
    lines(SDA | SCL);
    lines(SCL);
    lines(0);
    i2c_write(0x4Eu);
    i2c_write(value | 0x08u);
    lines(pins & (unsigned char)~SDA);
    lines(pins | SCL);
    lines(pins | SDA);
}

#define NIBBLE(value) do { expander((value) | 0x04u); expander(value); } while (0)
#define LCD_WRITE(value, rs) do { \
    NIBBLE(((value) & 0xF0u) | (rs)); \
    NIBBLE((unsigned char)((value) << 4) | (rs)); \
} while (0)

void main(void)
{
    TRISGPIO = 0b111100;
    GPIO = pins;

    NIBBLE(0x30u);
    NIBBLE(0x30u);
    NIBBLE(0x30u);
    NIBBLE(0x20u);
    LCD_WRITE(0x28u, 0);
    LCD_WRITE(0x0Cu, 0);
    LCD_WRITE(0x06u, 0);
    LCD_WRITE(0x01u, 0);

    LCD_WRITE('P', 1);
    LCD_WRITE('I', 1);
    LCD_WRITE('C', 1);
    LCD_WRITE('1', 1);
    LCD_WRITE('0', 1);
    LCD_WRITE('F', 1);
    LCD_WRITE('2', 1);
    LCD_WRITE('0', 1);
    LCD_WRITE('0', 1);

    for (;;) NOP();
}
