#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

enum {
    /*
     * 4～252 共 63 个亮度台阶，每级保持 8 个 PWM 周期。
     *
     * 旧示例只有 9 个亮度级，并且每级停留较久，因此肉眼会看到明显的
     * “走台阶”。这里把分辨率提高到原来的约 7 倍，同时缩短每级停留
     * 时间，让亮度连续变化。
     */
    PWM_STEP = 4,
    PWM_LEVEL_FRAMES = 8
};

/* 在当前占空比下输出若干个周期的软件 PWM。 */
static void pwm_level(unsigned char duty)
{
    unsigned char frames = PWM_LEVEL_FRAMES;
    unsigned char delay;

    /*
     * duty=0 必须单独处理。如果仍先把 GPIO 置 1，即使延时变量为 0，
     * GPIO 写指令和循环开销也会形成窄脉冲；真实 LED 在暗处对这些脉冲
     * 很敏感，看起来就会像“最低还有一半亮度”。
     *
     * 最暗端保持约 0.1 秒，让 LED 确实完全熄灭后再开始下一次渐亮。
     */
    if (duty == 0) {
        GPIO = 0;
        frames = 48;
        while (frames--) {
            delay = 255;
            while (delay--) {
                __asm("nop");
            }
        }
        return;
    }

    while (frames--) {
        GPIO = 1;
        delay = duty;
        while (delay--) {
            __asm("nop");
        }

        GPIO = 0;
        delay = (unsigned char)(255u - duty);
        while (delay--) {
            __asm("nop");
        }
    }
}

void main(void)
{
    unsigned char duty = PWM_STEP;
    unsigned char rising = 1;

    TRISGPIO = 0b111110; /* 只有 GP0 作为输出 */
    GPIO = 0;

    while (1) {
        pwm_level(duty);

        if (rising) {
            if (duty >= (unsigned char)(255u - PWM_STEP)) {
                rising = 0;
            } else {
                duty += PWM_STEP;
            }
        } else {
            if (duty <= PWM_STEP) {
                duty = 0;
                rising = 1;
            } else {
                duty -= PWM_STEP;
            }
        }
    }
}
