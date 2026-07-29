#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * 《致爱丽丝》开头旋律的简化版。
 *
 * PIC10F200没有PWM外设，这里用GP2软件翻转产生方波。每个音符的参数是
 * 半周期延时和重复次数；重复次数约等于“频率 × 0.12秒”。
 */
typedef enum {
    NOTE_E5,
    NOTE_DS5,
    NOTE_B4,
    NOTE_D5,
    NOTE_C5,
    NOTE_A4,
    NOTE_C4,
    NOTE_E4,
    NOTE_GS4
} Note;

static void play_note(Note note, unsigned char edges)
{
    /*
     * 一次循环产生一个半周期边沿。把延时选择写在本函数中，避免
     * main -> play_note -> delay三级调用超过PIC10F200两级硬件栈。
     */
    while (edges-- != 0) {
        GP2 = !GP2;
        switch (note) {
        case NOTE_E5:  __delay_us(759);  break; /* 659Hz */
        case NOTE_DS5: __delay_us(804);  break; /* 622Hz */
        case NOTE_B4:  __delay_us(1012); break; /* 494Hz */
        case NOTE_D5:  __delay_us(852);  break; /* 587Hz */
        case NOTE_C5:  __delay_us(956);  break; /* 523Hz */
        case NOTE_A4:  __delay_us(1136); break; /* 440Hz */
        case NOTE_C4:  __delay_us(1911); break; /* 262Hz */
        case NOTE_E4:  __delay_us(1517); break; /* 330Hz */
        default:       __delay_us(1204); break; /* G#4, 415Hz */
        }
    }

    GP2 = 0;
    /* 短暂断音，让相邻的相同或接近音符仍然能够区分。 */
    __delay_ms(30);
}

static void play_fur_elise(void)
{
    /* 基础音符约190ms，比原来的120ms更接近舒缓的演奏速度。 */
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_B4, 188);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 199);
    play_note(NOTE_A4, 255); /* 8位计数允许的最长乐句尾音 */

    __delay_ms(180);

    play_note(NOTE_C4, 115);
    play_note(NOTE_E4, 145);
    play_note(NOTE_A4, 194);
    play_note(NOTE_B4, 255);

    __delay_ms(180);

    play_note(NOTE_E4, 145);
    play_note(NOTE_GS4, 183);
    play_note(NOTE_B4, 217);
    play_note(NOTE_C5, 255);
}

void main(void)
{
    TRISGPIO = 0b111011; /* GP2连接无源蜂鸣器 */
    GPIO = 0;

    while (1) {
        play_fur_elise();
        __delay_ms(1000);
    }
}
