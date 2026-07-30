#include <xc.h>

/* 关闭看门狗，避免初始化和持续显示期间发生非预期复位。 */
#pragma config WDTE = OFF

/*
 * PIC10F200 + PCF8574 + LCD1602 软件 I2C 示例
 *
 * 硬件连接：
 *   GP0 -> SDA
 *   GP1 -> SCL
 *
 * PIC10F200 没有硬件 I2C，下面直接翻转 GPIO 产生总线时序。Web 器件模拟
 * 地址为 0x27 的 PCF8574 I/O 扩展器，扩展器再以 4 位模式驱动 LCD1602。
 */
#define SDA 0x01u
#define SCL 0x02u

/* 保存 GP0/GP1 的输出影子值，初始高电平表示总线空闲。 */
static unsigned char pins = SDA | SCL;

/*
 * 同时更新影子值和 GPIO。额外的 NOP 为模拟器件留出一个明确的稳定周期，
 * 使每次边沿都能被外设模型可靠采样。
 */
#define lines(value) do { pins = (value); GPIO = pins; NOP(); } while (0)

/* 按最高位优先发送一个字节，并产生第九个 ACK 时钟。 */
static void i2c_write(unsigned char value)
{
    unsigned char bits = 8;
    do {
        /* SCL 为低时准备当前数据位，保证数据在时钟上升沿前稳定。 */
        lines((pins & (unsigned char)~(SDA | SCL)) |
              ((value & 0x80u) ? SDA : 0));

        /* 拉高 SCL 让从设备采样数据，再拉低以进入下一位。 */
        lines(pins | SCL);
        lines(pins & (unsigned char)~SCL);
        value <<= 1;
    } while (--bits);

    /*
     * 释放 SDA 并产生第九个时钟作为 ACK 周期。本示例不读取 ACK，
     * 只演示最精简的单向写入流程。
     */
    lines(pins | SDA);
    lines(pins | SCL);
    lines(pins & (unsigned char)~SCL);
}

/* 完成一次 I2C 事务，把一个字节写入 PCF8574。 */
static void expander(unsigned char value)
{
    /* 总线空闲后，在 SCL 为高时把 SDA 从高拉低，形成 START 条件。 */
    lines(SDA | SCL);
    lines(SCL);
    lines(0);

    /* 0x4E = 7 位地址 0x27 左移一位，加上写方向位 0。 */
    i2c_write(0x4Eu);

    /* 位 3 为 LCD 背光控制，本例始终置 1 以保持背光开启。 */
    i2c_write(value | 0x08u);

    /* 在 SCL 为高时把 SDA 从低释放到高，形成 STOP 条件。 */
    lines(pins & (unsigned char)~SDA);
    lines(pins | SCL);
    lines(pins | SDA);
}

/* 位 2 对应 LCD 的 E 使能端：先置位再清零，用下降沿锁存一个半字节。 */
#define NIBBLE(value) do { expander((value) | 0x04u); expander(value); } while (0)

/*
 * LCD 工作在 4 位模式，一个字节需先发高半字节、再发低半字节。
 * rs=0 选择指令寄存器，rs=1 选择数据寄存器。
 */
#define LCD_WRITE(value, rs) do { \
    NIBBLE(((value) & 0xF0u) | (rs)); \
    NIBBLE((unsigned char)((value) << 4) | (rs)); \
} while (0)

void main(void)
{
    /* GP0、GP1 用于软件 I2C 输出；GP2、GP3 保持输入。 */
    TRISGPIO = 0b111100;

    /* SDA、SCL 初始均为高电平，对应 I2C 总线空闲状态。 */
    GPIO = pins;

    /*
     * HD44780 兼容控制器的 4 位初始化序列：前三次 0x3 强制进入已知的
     * 8 位初始化状态，最后一次 0x2 切换到 4 位传输模式。
     */
    NIBBLE(0x30u);
    NIBBLE(0x30u);
    NIBBLE(0x30u);
    NIBBLE(0x20u);
    /* 0x28：4 位总线、双行显示、5×8 点阵字符。 */
    LCD_WRITE(0x28u, 0);
    /* 0x0C：开启显示，关闭光标和光标闪烁。 */
    LCD_WRITE(0x0Cu, 0);
    /* 0x06：每写入一个字符后，显示地址自动向右递增。 */
    LCD_WRITE(0x06u, 0);
    /* 0x01：清屏，并把显示地址恢复到首字符位置。 */
    LCD_WRITE(0x01u, 0);

    /* rs=1 表示写字符数据，依次在第一行显示“PIC10F200”。 */
    LCD_WRITE('P', 1);
    LCD_WRITE('I', 1);
    LCD_WRITE('C', 1);
    LCD_WRITE('1', 1);
    LCD_WRITE('0', 1);
    LCD_WRITE('F', 1);
    LCD_WRITE('2', 1);
    LCD_WRITE('0', 1);
    LCD_WRITE('0', 1);

    /* 内容写入后无需反复刷新，保持空循环即可让 LCD 持续显示。 */
    for (;;) {
        NOP();
    }
}
