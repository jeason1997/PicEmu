#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * GP0 软件 PWM 呼吸灯。
 *
 * 每个亮度台阶保持若干个 PWM 周期，随后逐渐增加或减少占空比。
 * Tang Nano 1K 顶层把 GP0 映射到板载红色 LED；同一份 HEX 在 SDL
 * 中也会根据占空比显示渐亮和渐灭。
 */
static void pwm_level(unsigned char on_ticks, unsigned char off_ticks)
{
    unsigned char frames = 80;
    unsigned char delay;

    while (frames--) {
        GPIO = 1;
        delay = on_ticks;
        while (delay--) {
            __asm("nop");
        }

        GPIO = 0;
        delay = off_ticks;
        while (delay--) {
            __asm("nop");
        }
    }
}

void main(void)
{
    TRISGPIO = 0b111110; /* 只有 GP0 作为输出 */
    GPIO = 0;

    while (1) {
        /* 渐亮。每组之和保持 256，使 PWM 频率基本不变。 */
        pwm_level(8,   248);
        pwm_level(32,  224);
        pwm_level(64,  192);
        pwm_level(96,  160);
        pwm_level(128, 128);
        pwm_level(160, 96);
        pwm_level(192, 64);
        pwm_level(224, 32);
        pwm_level(248, 8);

        /* 渐灭，不重复最亮和最暗台阶。 */
        pwm_level(224, 32);
        pwm_level(192, 64);
        pwm_level(160, 96);
        pwm_level(128, 128);
        pwm_level(96,  160);
        pwm_level(64,  192);
        pwm_level(32,  224);
    }
}
