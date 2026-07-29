#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

void main(void)
{
    TRISGPIO = 0b111110; /* 只有GP0作为输出 */
    GPIO = 0;

    while (1) {
        GPIO = 0b000001;
        __delay_ms(1000);

        GPIO = 0b000000;
        __delay_ms(1000);
    }
}
