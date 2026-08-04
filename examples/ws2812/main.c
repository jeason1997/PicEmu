#include <xc.h>
#include <stdint.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF
#pragma config CP = OFF
#pragma config MCLRE = OFF

/*
 * 矩阵配置：Z 型表示每一行都从左向右编号，S 型表示奇数行反向编号。
 * PIC10F200 的 RAM 很小，本示例使用 8 位索引，因此总灯数不能超过 255。
 */
#ifndef WS2812_ROWS
#define WS2812_ROWS 8u
#endif
#ifndef WS2812_COLS
#define WS2812_COLS 8u
#endif
#ifndef WS2812_WIRING_S
#define WS2812_WIRING_S 0u /* 0：Z 型（默认）；1：S 型（蛇形）。 */
#endif
#define WS2812_COUNT (WS2812_ROWS * WS2812_COLS)

#if WS2812_COUNT > 255u
#error "PIC10F200 WS2812 example supports at most 255 pixels"
#endif

/*
 * PIC10F200 的 4 MHz 时钟意味着一个指令周期为 1 us，不能严格生成新版
 * WS2812B 标称的 0.4/0.8 us 高电平。本教学例程用 1/2 个指令周期区分
 * 0 和 1，适合 PicEmu 的宽容时序模型；连接实物前应改用更高主频 MCU，
 * 或确认手中灯珠能够接受这种放宽时序。
 */
#define WS2812_WRITE_BIT(one) \
    do                        \
    {                         \
        if (one)              \
        {                     \
            GP0 = 1;          \
            NOP();            \
            GP0 = 0;          \
        }                     \
        else                  \
        {                     \
            GP0 = 1;          \
            GP0 = 0;          \
        }                     \
        NOP();                \
    } while (0)

static void ws2812_write_byte(uint8_t value)
{
    uint8_t bit;
    for (bit = 0; bit < 8; ++bit)
    {
        WS2812_WRITE_BIT((value & 0x80u) != 0);
        value <<= 1;
    }
}

static void ws2812_show(void)
{
    GP0 = 0;
    /* 与仿真模型及新版 WS2812B 的复位时间保持一致。 */
    __delay_us(300);
}

void main(void)
{
    uint8_t physical;
    uint8_t logical;
    uint8_t phase = 0;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    TRISGPIO = 0x0Eu; /* GP0 输出数据，GP1～GP3 保持输入。 */
    GPIO = 0;

    for (;;)
    {
        for (physical = 0; physical < WS2812_COUNT; ++physical)
        {
            logical = physical;
#if WS2812_WIRING_S
            if (((physical / WS2812_COLS) & 1u) != 0)
            {
                logical = (physical / WS2812_COLS) * WS2812_COLS +
                          (WS2812_COLS - 1u - physical % WS2812_COLS);
            }
#endif
            switch ((logical + phase) & 3u)
            {
            case 0:
                red = 255;
                green = 0;
                blue = 0;
                break;
            case 1:
                red = 0;
                green = 255;
                blue = 0;
                break;
            case 2:
                red = 0;
                green = 0;
                blue = 255;
                break;
            default:
                red = 96;
                green = 32;
                blue = 0;
                break;
            }
            /* 线上固定为 GRB；三次调用之间不能插入复位级低电平。 */
            ws2812_write_byte(green);
            ws2812_write_byte(red);
            ws2812_write_byte(blue);
        }
        ws2812_show();
        __delay_ms(1000);
        phase ^= 2u;
    }
}
