#include <xc.h>

#define _XTAL_FREQ 4000000

/* 关闭看门狗，保证循环延时不会引发周期性复位。 */
#pragma config WDTE = OFF

/*
 * PIC10F200 三路流水灯。
 *
 * GP0、GP1、GP2 都配置为输出，GP3 保持输入。
 * 在 Tang Nano 1K FPGA 顶层中，它们分别连接板载红、蓝、绿三颗 LED。
 * 程序每隔 100 ms 把唯一的高电平移动到下一路，形成循环追逐效果。
 */
void main(void)
{
    /* TRIS 位为 0 代表输出，因此低三位清零可同时启用三路 LED。 */
    TRISGPIO = 0b111000;

    /* 初始化时先关闭全部 LED，避免方向切换期间出现不确定电平。 */
    GPIO = 0;

    while (1) {
        /* 仅 GP0 为高：点亮第一颗 LED。 */
        GPIO = 0b000001;
        __delay_ms(100);

        /* 仅 GP1 为高：点亮第二颗 LED。 */
        GPIO = 0b000010;
        __delay_ms(100);

        /* 仅 GP2 为高：点亮第三颗 LED，随后循环回到 GP0。 */
        GPIO = 0b000100;
        __delay_ms(100);
    }
}
