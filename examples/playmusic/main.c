#include <xc.h>

#define _XTAL_FREQ 4000000

#pragma config WDTE = OFF

/*
 * PIC10F202 单声道蜂鸣器音乐示例。
 *
 * GP2 通过软件翻转输出方波。PIC10F202 没有 PWM，且程序存储器只有
 * 512 个字，因此这里保存《致爱丽丝》中较长且最容易辨认的主旋律，
 * 不包含钢琴原曲的和弦、低音伴奏和力度变化。
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
    NOTE_GS4,
    NOTE_G4,
    NOTE_F5,
    NOTE_F4
} Note;

static void play_note(Note note, unsigned char edges)
{
    /*
     * 一次循环产生一个半周期。延时直接放在本函数内，使调用深度保持为
     * main -> play_music -> play_note，适配 PIC10F202 的两级硬件栈。
     */
    while (edges-- != 0) {
        GP2 = !GP2;
        switch (note) {
        case NOTE_E5:  __delay_us(759);  break; /* 659 Hz */
        case NOTE_DS5: __delay_us(804);  break; /* 622 Hz */
        case NOTE_B4:  __delay_us(1012); break; /* 494 Hz */
        case NOTE_D5:  __delay_us(852);  break; /* 587 Hz */
        case NOTE_C5:  __delay_us(956);  break; /* 523 Hz */
        case NOTE_A4:  __delay_us(1136); break; /* 440 Hz */
        case NOTE_C4:  __delay_us(1911); break; /* 262 Hz */
        case NOTE_E4:  __delay_us(1517); break; /* 330 Hz */
        case NOTE_GS4: __delay_us(1204); break; /* 415 Hz */
        case NOTE_G4:  __delay_us(1276); break; /* 392 Hz */
        case NOTE_F5:  __delay_us(716);  break; /* 698 Hz */
        default:       __delay_us(1433); break; /* F4, 349 Hz */
        }
    }

    GP2 = 0;
    /*
     * 每个音符后留出基本断音。原来的 25 ms 会让旋律显得过于急促，
     * 45 ms 更接近舒缓的钢琴演奏速度。
     */
    __delay_ms(45);
}

static void rest(unsigned char units)
{
    /* 以 10 ms 为单位，可用很少的程序字表达不同长度的乐句休止。 */
    while (units-- != 0) {
        __delay_ms(10);
    }
}

static void play_music(void)
{
    /*
     * 短音约 0.22 秒，255 边沿的音约 0.25～0.49 秒。
     * 旋律由《致爱丽丝》主题的两个较长乐句组成。
     */
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_B4, 188);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 199);
    play_note(NOTE_A4, 255);
    rest(18); /* 第一小句结束：180 ms。 */

    play_note(NOTE_C4, 115);
    play_note(NOTE_E4, 145);
    play_note(NOTE_A4, 194);
    play_note(NOTE_B4, 255);
    rest(10);
    play_note(NOTE_E4, 145);
    play_note(NOTE_GS4, 183);
    play_note(NOTE_B4, 217);
    play_note(NOTE_C5, 255);
    rest(22); /* 第一乐句结束。 */

    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_B4, 188);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 199);
    play_note(NOTE_A4, 255);
    rest(18);

    play_note(NOTE_C4, 115);
    play_note(NOTE_E4, 145);
    play_note(NOTE_A4, 194);
    play_note(NOTE_B4, 255);
    play_note(NOTE_E4, 145);
    play_note(NOTE_C5, 199);
    play_note(NOTE_B4, 217);
    play_note(NOTE_A4, 255);
    rest(26); /* 主题段结束，进入展开乐句。 */

    play_note(NOTE_B4, 188);
    play_note(NOTE_C5, 199);
    play_note(NOTE_D5, 223);
    play_note(NOTE_E5, 255);
    play_note(NOTE_G4, 165);
    play_note(NOTE_F5, 230);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 255);
    rest(12);
    play_note(NOTE_F4, 155);
    play_note(NOTE_E5, 250);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 255);

    play_note(NOTE_E4, 145);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 199);
    play_note(NOTE_B4, 255);
    play_note(NOTE_E4, 145);
    play_note(NOTE_E5, 250);
    play_note(NOTE_E4, 145);
    play_note(NOTE_E5, 250);
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 255);
    rest(26); /* 展开段结束，稍长停顿后回到主题。 */

    /* 回到开头主题，以 PIC10F202 剩余程序空间延长可辨识旋律。 */
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_DS5, 236);
    play_note(NOTE_E5, 250);
    play_note(NOTE_B4, 188);
    play_note(NOTE_D5, 223);
    play_note(NOTE_C5, 199);
    play_note(NOTE_A4, 255);
    rest(18);
    play_note(NOTE_C4, 115);
    play_note(NOTE_E4, 145);
    play_note(NOTE_A4, 194);
    play_note(NOTE_B4, 255);
    play_note(NOTE_E4, 145);
    play_note(NOTE_C5, 199);
    play_note(NOTE_B4, 217);
    play_note(NOTE_A4, 255);
    rest(40); /* 整段结束。 */
}

void main(void)
{
    TRISGPIO = 0b111011; /* GP2 输出并连接无源蜂鸣器。 */
    GPIO = 0;

    while (1) {
        play_music();
        __delay_ms(1000);
    }
}
