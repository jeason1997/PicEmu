#include <xc.h>

#define _XTAL_FREQ 4000000

/* 关闭看门狗，避免较长的乐曲播放过程被复位打断。 */
#pragma config WDTE = OFF

/*
 * PIC10F200 无源蜂鸣器音乐示例。
 *
 * 芯片没有 PWM 外设，因此程序通过 GP2 软件翻转产生方波。为了装入
 * 256 字的程序存储器，示例只使用四个音高，播放一段简单的原创旋律。
 */
typedef enum {
    /* 枚举值只用于选择对应半周期延时，不直接表示频率数值。 */
    NOTE_C5,
    NOTE_D5,
    NOTE_E5,
    NOTE_F5
} Note;

static void play_note(Note note, unsigned char edges)
{
    /*
     * 每次循环翻转一次 GP2，因此产生一个方波边沿，也就是半个周期。
     * edges 决定音符持续的半周期数量。延时直接放在本函数中，使最大调用深度为
     * main -> play_music -> play_note，不超过 PIC10F200 的两级硬件栈。
     */
    while (edges-- != 0) {
        /* 翻转输出电平，连续两次翻转构成一个完整方波周期。 */
        GP2 = !GP2;

        /*
         * 延时值约等于目标频率半周期的微秒数。switch 和循环本身也有少量
         * 指令开销，因此这是适合演示的近似音高，而不是精密音频发生器。
         */
        switch (note) {
        case NOTE_C5: __delay_us(956); break; /* 523 Hz */
        case NOTE_D5: __delay_us(852); break; /* 587 Hz */
        case NOTE_E5: __delay_us(759); break; /* 659 Hz */
        default:      __delay_us(716); break; /* F5，698 Hz */
        }
    }

    /* 每个音符结束后强制回到低电平，避免蜂鸣器停在高电平。 */
    GP2 = 0;

    /* 插入 55 ms 静音，使相邻音符的起止更容易辨认。 */
    __delay_ms(55);
}

static void rest(unsigned char units)
{
    /*
     * 以 10 ms 为单位生成乐句之间的停顿。拆成循环可让 unsigned char
     * 参数覆盖更长的休止时间，同时避免依赖可变参数延时宏。
     */
    while (units-- != 0) {
        __delay_ms(10);
    }
}

static void play_music(void)
{
    /*
     * 第一乐句。edges 接近 250，表示每个音符约输出 125 个完整周期；
     * 不同频率下实际时长略有差异，这是极小程序空间下的有意取舍。
     */
    play_note(NOTE_E5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_C5, 255);
    rest(18);

    /* 第二乐句；连续播放两次相同尾音，用有限接口表达较长音符。 */
    play_note(NOTE_C5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_D5, 255);
    rest(25);

    /* 最后一段先上行再下行，并用较长休止标记整轮旋律结束。 */
    play_note(NOTE_C5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_C5, 255);
    rest(40);
}

void main(void)
{
    /* 仅将连接无源蜂鸣器的 GP2 配置为输出，其余引脚保持输入。 */
    TRISGPIO = 0b111011;

    /* 初始输出低电平，保证播放开始前蜂鸣器静音。 */
    GPIO = 0;

    while (1) {
        /* 循环播放完整旋律，适合持续观察模拟器中的 GPIO 波形。 */
        play_music();

        /* 100 × 10 ms = 1 秒，作为两轮旋律之间的间隔。 */
        rest(100);
    }
}
