#include <xc.h>

#define _XTAL_FREQ 4000000

/* 关闭看门狗，避免软件 PWM 的忙等待循环触发复位。 */
#pragma config WDTE = OFF

/*
 * PIC10F200 软件 PWM 呼吸灯示例
 *
 * 硬件连接：
 *   GP0 -> LED（高电平点亮）
 *
 * 芯片没有硬件 PWM，程序用两个忙等待区段分别表示一个周期中的亮、灭时间。
 * duty 越大，高电平持续越久，LED 的平均亮度越高。
 */
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
    /* frames 表示当前亮度要重复多少个完整 PWM 周期。 */
    unsigned char frames = PWM_LEVEL_FRAMES;

    /* delay 是 8 位递减计数器，用循环耗时近似控制高、低电平宽度。 */
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
                /* 空操作只消耗一个指令周期，不改变 GPIO 或程序状态。 */
                __asm("nop");
            }
        }
        return;
    }

    while (frames--) {
        /* 高电平区段长度与 duty 成正比。 */
        GPIO = 1;
        delay = duty;
        while (delay--) {
            __asm("nop");
        }

        /* 低电平区段补足到约 255 个计数，使 PWM 总周期基本稳定。 */
        GPIO = 0;
        delay = (unsigned char)(255u - duty);
        while (delay--) {
            __asm("nop");
        }
    }
}

void main(void)
{
    /* 从最低的非零亮度开始，避免上电后直接跳到较亮状态。 */
    unsigned char duty = PWM_STEP;

    /* rising 为 1 表示渐亮，为 0 表示渐暗。 */
    unsigned char rising = 1;

    /* 只有连接 LED 的 GP0 配置为输出，其余引脚保持输入。 */
    TRISGPIO = 0b111110;
    GPIO = 0;

    while (1) {
        /* 先以当前占空比输出若干周期，再计算下一个亮度台阶。 */
        pwm_level(duty);

        if (rising) {
            /*
             * 8 位 duty 最大为 255。提前在 255-PWM_STEP 处改变方向，
             * 可避免加法溢出后从高亮度跳回低亮度。
             */
            if (duty >= (unsigned char)(255u - PWM_STEP)) {
                rising = 0;
            } else {
                duty += PWM_STEP;
            }
        } else {
            if (duty <= PWM_STEP) {
                /*
                 * 渐暗到最低一级后插入 duty=0 的全灭阶段；下一轮调用
                 * pwm_level(0) 完成暗端停顿，然后重新开始渐亮。
                 */
                duty = 0;
                rising = 1;
            } else {
                duty -= PWM_STEP;
            }
        }
    }
}
