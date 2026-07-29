#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

void main(void)
{
    unsigned char led_state = 0b000001;

    /*
     * GP3：按键输入（PIC10F200的GP3本来就只能输入）
     * GP2：蜂鸣器输出
     * GP1：LED2输出
     * GP0：LED1输出
     */
    TRISGPIO = 0b111000;
    GPIO = led_state;

    while (1) {
        /* 按键为低电平有效。 */
        if (GP3 == 0) {
            /* 简单消抖，避免机械按键抖动产生多次触发。 */
            __delay_ms(20);
            if (GP3 == 0) {
                /* GP0和GP1同时取反。 */
                led_state ^= 0b000011;

                /* 保留LED状态，同时让GP2蜂鸣器响50ms。 */
                GPIO = led_state | 0b000100;
                __delay_ms(50);
                GPIO = led_state;

                /*
                 * 等待按键松开。因此无论按住多久，本次按下都只触发一次。
                 */
                while (GP3 == 0) {
                    NOP();
                }
                __delay_ms(20);
            }
        }
    }
}
