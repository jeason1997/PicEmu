#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * PIC10F200 + 74HC595 + 单位七段数码管示例。
 *
 * GP0 -> 74HC595 SER（串行数据）
 * GP1 -> 74HC595 SRCLK（移位时钟）
 * GP2 -> 74HC595 RCLK（输出锁存）
 *
 * 段码低 8 位依次为 a、b、c、d、e、f、g、dp。三线串行驱动既符合 PIC10
 * 仅有三个可输出 GPIO 的真实限制，也能避免把某个数字的结果写死在模拟器中。
 */
#define DATA_MASK  0x01u
#define CLOCK_MASK 0x02u
#define LATCH_MASK 0x04u

static unsigned char outputs;

static const unsigned char digit_segments[10] = {
    0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u,
    0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu
};

static void write_outputs(unsigned char value)
{
    outputs = value;
    GPIO = outputs;
    NOP();
}

static void display_write(unsigned char segments)
{
    unsigned char bits = 8;

    /* 最高位优先发送；每一位在 CLOCK 上升沿之前保持稳定。 */
    do {
        write_outputs((unsigned char)(outputs & ~(DATA_MASK | CLOCK_MASK)) |
                      ((segments & 0x80u) ? DATA_MASK : 0u));
        write_outputs(outputs | CLOCK_MASK);
        write_outputs(outputs & (unsigned char)~CLOCK_MASK);
        segments <<= 1;
    } while (--bits);

    /* 锁存上升沿一次性更新八个段，移位期间屏幕保持旧数字。 */
    write_outputs(outputs | LATCH_MASK);
    write_outputs(outputs & (unsigned char)~LATCH_MASK);
}

void main(void)
{
    unsigned char digit = 0;

    TRISGPIO = 0b111000;
    write_outputs(0);

    for (;;) {
        display_write(digit_segments[digit]);
        __delay_ms(250);
        if (++digit == 10u) digit = 0;
    }
}
