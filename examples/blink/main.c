#include <xc.h>

#define _XTAL_FREQ 4000000

/* 关闭看门狗，避免教学示例因未定期清看门狗而意外复位。 */
#pragma config WDTE = OFF

/*
 * PIC10F200 单路 LED 闪烁示例
 *
 * 硬件连接：
 *   GP0 -> LED（高电平点亮）
 *
 * PIC10F200 上电后 GPIO 默认并不都是输出，必须先通过 TRISGPIO 明确设置
 * 方向。程序随后以 1 秒为间隔交替输出高、低电平，形成稳定的闪烁效果。
 */
void main(void)
{
    /*
     * TRISGPIO 中某位为 1 表示输入、为 0 表示输出。
     * 这里只把 GP0 配置为输出；GP1、GP2 和只能输入的 GP3 均保持输入。
     */
    TRISGPIO = 0b111110;

    /* 先输出低电平，确保初始化阶段 LED 处于熄灭状态。 */
    GPIO = 0;

    while (1) {
        /* GP0 置 1，点亮 LED，并保持 1 秒。 */
        GPIO = 0b000001;
        __delay_ms(1000);

        /* GP0 清 0，熄灭 LED，并保持 1 秒。 */
        GPIO = 0b000000;
        __delay_ms(1000);
    }
}
