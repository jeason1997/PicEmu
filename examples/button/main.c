#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

void main(void)
{
    unsigned char led_state = 0b000001;

    TRISGPIO = 0b111100; /* GP0、GP1输出，GP3按键输入 */
    GPIO = led_state;

    while (1) {
        if (GP3 == 0) {
            __delay_ms(20);
            if (GP3 == 0) {
                led_state ^= 0b000011;
                GPIO = led_state;

                while (GP3 == 0) {
                    NOP();
                }
                __delay_ms(20);
            }
        }
    }
}
