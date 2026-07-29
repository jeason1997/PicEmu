#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

void main(void)
{
    TRISGPIO = 0b111000;
    GPIO = 0;

    while (1) {
        GPIO = 0b000001;
        __delay_ms(5);

        GPIO = 0b000010;
        __delay_ms(5);

        GPIO = 0b000100;
        __delay_ms(5);
    }
}