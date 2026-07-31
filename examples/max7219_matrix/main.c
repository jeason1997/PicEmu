#include <xc.h>
#include <stdint.h>

#pragma config WDTE = OFF
#pragma config CP = OFF
#pragma config MCLRE = OFF

#define MAX_LOAD 0x04u

static void max7219_write(uint8_t address, uint8_t value)
{
    uint8_t bit;
    GP2 = 0;
    for (bit = 0; bit < 8; ++bit) {
        GP0 = (address & 0x80u) != 0;
        GP1 = 1; GP1 = 0;
        address <<= 1;
    }
    for (bit = 0; bit < 8; ++bit) {
        GP0 = (value & 0x80u) != 0;
        GP1 = 1; GP1 = 0;
        value <<= 1;
    }
    GP2 = 1;
}

void main(void)
{
    GPIO = MAX_LOAD;
    TRISGPIO = 0x08u; /* GP3 只能输入，GP0～GP2 用于软件串行接口。 */
    max7219_write(0x0Fu, 0); /* 关闭显示测试。 */
    max7219_write(0x09u, 0); /* 点阵使用无译码模式。 */
    max7219_write(0x0Bu, 7); /* 扫描全部八行。 */
    max7219_write(0x0Au, 5); /* 中等亮度。 */
    max7219_write(0x0Cu, 1); /* 退出关断模式。 */

    /*
     * XC8 4.00 的 baseline PIC 代码生成器会在“循环内调用位移发送函数”时
     * 陷入异常耗时的分析，因此明确展开固定的八行。生成代码仍共享发送函数，
     * 程序空间开销很小，同时不会改变 MAX7219 的真实串行时序。
     */
    max7219_write(1, 0x3Cu);
    max7219_write(2, 0x42u);
    max7219_write(3, 0xA5u);
    max7219_write(4, 0x81u);
    max7219_write(5, 0xA5u);
    max7219_write(6, 0x99u);
    max7219_write(7, 0x42u);
    max7219_write(8, 0x3Cu);
    for (;;) { }
}
