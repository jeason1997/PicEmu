#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * PIC10F200 无源蜂鸣器音乐示例。
 *
 * 芯片没有 PWM 外设，因此程序通过 GP2 软件翻转产生方波。为了装入
 * 256 字的程序存储器，示例只使用四个音高，播放一段简单的原创旋律。
 */
typedef enum {
    NOTE_C5,
    NOTE_D5,
    NOTE_E5,
    NOTE_F5
} Note;

static void play_note(Note note, unsigned char edges)
{
    /*
     * 每次循环产生一个半周期。延时直接放在本函数中，使最大调用深度为
     * main -> play_music -> play_note，不超过 PIC10F200 的两级硬件栈。
     */
    while (edges-- != 0) {
        GP2 = !GP2;

        switch (note) {
        case NOTE_C5: __delay_us(956); break; /* 523 Hz */
        case NOTE_D5: __delay_us(852); break; /* 587 Hz */
        case NOTE_E5: __delay_us(759); break; /* 659 Hz */
        default:      __delay_us(716); break; /* F5，698 Hz */
        }
    }

    GP2 = 0;
    __delay_ms(55); /* 音符之间的断音。 */
}

static void rest(unsigned char units)
{
    /* 以 10 ms 为单位生成乐句之间的停顿。 */
    while (units-- != 0) {
        __delay_ms(10);
    }
}

static void play_music(void)
{
    /* 第一乐句。 */
    play_note(NOTE_E5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_F5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_C5, 255);
    rest(18);

    /* 第二乐句。连续播放两次尾音，表示较长音符。 */
    play_note(NOTE_C5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 250);
    play_note(NOTE_D5, 255);
    rest(25);

    /* 用一个短小的上行、下行乐句结束。 */
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
    TRISGPIO = 0b111011; /* GP2 输出并连接无源蜂鸣器。 */
    GPIO = 0;

    while (1) {
        play_music();
        rest(100); /* 每轮旋律之间停顿一秒。 */
    }
}
