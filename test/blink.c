#include <xc.h>

#pragma config WDTE = OFF

void delay(unsigned int count) {
    while(count--) {
        __asm("nop");
    }
}

void main(void) {
    TRISGPIO = 0b111100;
    GPIO = 0b000000;
    
    while(1) {
        GP0 = 1;
        GP1 = 0;
        delay(50000);
        GP0 = 0;
        GP1 = 1;
        delay(50000);
    }
}
