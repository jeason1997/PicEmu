#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

void main(void)
{
    TRISGPIO = 0b111011; /* GP2输出，GP3按键输入 */
    GPIO = 0;

    while (1) {
        if (GP3 == 0) {
            __delay_ms(20);
            if (GP3 == 0) {
                GP2 = 1;
                __delay_ms(50);
                GP2 = 0;

                while (GP3 == 0) {
                    NOP();
                }
                __delay_ms(20);
            }
        }
    }
}
