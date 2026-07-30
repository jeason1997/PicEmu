#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * PIC10F200 三路流水灯。
 *
 * GP0、GP1、GP2 都配置为输出，GP3 保持输入。
 * 在 Tang Nano 1K FPGA 顶层中，它们分别连接板载红、蓝、绿三颗 LED。
 */
void main(void)
{
    TRISGPIO = 0b111000;
    GPIO = 0;

    while (1) {
        GPIO = 0b000001;
        __delay_ms(100);

        GPIO = 0b000010;
        __delay_ms(100);

        GPIO = 0b000100;
        __delay_ms(100);
    }
}
