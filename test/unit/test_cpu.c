#include "disassembler.h"
#include "pic10f200.h"
#include "sim_board.h"
#include "sim_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: 检查失败：%s\n",                       \
                    __FILE__, __LINE__, #condition);                         \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static HexImage blank_image(bool watchdog_enabled)
{
    HexImage image;
    unsigned i;

    memset(&image, 0, sizeof(image));
    for (i = 0; i < PIC10F200_PROGRAM_WORDS; ++i) {
        image.program[i] = 0x000; /* NOP */
    }
    image.config_present = true;
    image.config_word = watchdog_enabled ? 0x0FFFu : 0x0FFBu;
    return image;
}

static void test_addwf_flags(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    /* ADDWF 0x10,F */
    image.program[0] = 0x1C0u | 0x20u | 0x10u;
    pic10f200_init(&cpu, &image);
    cpu.w = 0x01;
    cpu.ram[0x10] = 0xFF;
    pic10f200_step(&cpu);

    CHECK(cpu.ram[0x10] == 0x00);
    CHECK((cpu.ram[PIC10_STATUS] & 0x01u) != 0); /* C */
    CHECK((cpu.ram[PIC10_STATUS] & 0x02u) != 0); /* DC */
    CHECK((cpu.ram[PIC10_STATUS] & 0x04u) != 0); /* Z */
}

static void test_subwf_borrow(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    /* SUBWF 0x10,W：0x01 - 0x02 = 0xFF，产生借位时C清零。 */
    image.program[0] = 0x080u | 0x10u;
    pic10f200_init(&cpu, &image);
    cpu.w = 0x02;
    cpu.ram[0x10] = 0x01;
    pic10f200_step(&cpu);

    CHECK(cpu.w == 0xFF);
    CHECK((cpu.ram[PIC10_STATUS] & 0x01u) == 0);
    CHECK((cpu.ram[PIC10_STATUS] & 0x04u) == 0);
}

static void test_skip_cycles(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;
    Pic10StepResult result;

    /* BTFSC 0x10,0：bit为0时跳过下一条。 */
    image.program[0] = 0x600u | 0x10u;
    image.program[1] = 0xCAA; /* MOVLW 0xAA，不应执行 */
    image.program[2] = 0xC55; /* MOVLW 0x55 */
    pic10f200_init(&cpu, &image);

    result = pic10f200_step(&cpu);
    CHECK(result.instruction_cycles == 2);
    CHECK(cpu.pc == 2);
    pic10f200_step(&cpu);
    CHECK(cpu.w == 0x55);
}

static void test_call_retlw(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    image.program[0] = 0x905; /* CALL 0x05 */
    image.program[1] = 0xC42; /* 返回后执行 */
    image.program[5] = 0x899; /* RETLW 0x99 */
    pic10f200_init(&cpu, &image);

    pic10f200_step(&cpu);
    CHECK(cpu.pc == 5);
    pic10f200_step(&cpu);
    CHECK(cpu.pc == 1);
    CHECK(cpu.w == 0x99);
    pic10f200_step(&cpu);
    CHECK(cpu.w == 0x42);
}

static void test_indirect_addressing(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    /* MOVWF INDF，FSR指向0x10。 */
    image.program[0] = 0x020;
    pic10f200_init(&cpu, &image);
    cpu.ram[PIC10_FSR] = 0x10;
    cpu.w = 0xA5;
    pic10f200_step(&cpu);

    CHECK(cpu.ram[0x10] == 0xA5);
    CHECK(pic10f200_read_register(&cpu, PIC10_INDF) == 0xA5);
}

static void test_timer0_write_inhibit(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    image.program[0] = 0x020 | PIC10_TMR0; /* MOVWF TMR0 */
    pic10f200_init(&cpu, &image);
    cpu.option = 0x08; /* 内部时钟，预分频器给WDT，TMR0每周期+1 */
    cpu.w = 10;

    pic10f200_step(&cpu); /* 写入周期 */
    pic10f200_step(&cpu); /* 抑制1 */
    pic10f200_step(&cpu); /* 抑制2 */
    CHECK(cpu.ram[PIC10_TMR0] == 10);
    pic10f200_step(&cpu);
    CHECK(cpu.ram[PIC10_TMR0] == 11);
}

static void test_watchdog_wakes_sleep(void)
{
    HexImage image = blank_image(true);
    Pic10F200 cpu;

    image.program[0] = 0x003; /* SLEEP */
    pic10f200_init(&cpu, &image);
    cpu.watchdog_base_period = 2; /* 测试中缩短标称18ms超时 */
    cpu.option = 0x00;            /* PSA给TMR0，WDT为1:1 */

    pic10f200_step(&cpu);
    CHECK(cpu.sleeping);
    pic10f200_step(&cpu);
    CHECK(!cpu.sleeping);
    CHECK((cpu.ram[PIC10_STATUS] & (1u << PIC10_STATUS_TO)) == 0);
    CHECK((cpu.ram[PIC10_STATUS] & (1u << PIC10_STATUS_PD)) == 0);
}

static void test_external_t0cki_and_pin_wakeup(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    pic10f200_init(&cpu, &image);
    cpu.option = 0x68; /* T0CS=1、上升沿、PSA=1、弱上拉关闭 */
    cpu.tris_gpio = 0x0F;
    pic10f200_drive_pin(&cpu, 2, true, false);
    pic10f200_drive_pin(&cpu, 2, true, true);
    CHECK(cpu.ram[PIC10_TMR0] == 1);

    cpu.sleeping = true;
    cpu.option = 0x40; /* GPWU=0，允许引脚变化唤醒 */
    pic10f200_drive_pin(&cpu, 3, true, false);
    pic10f200_drive_pin(&cpu, 3, true, true);
    CHECK(!cpu.sleeping);
}

static void test_status_read_only_bits(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;

    /* MOVWF STATUS不应清除只读的TO和PD。 */
    image.program[0] = 0x020u | PIC10_STATUS;
    pic10f200_init(&cpu, &image);
    cpu.w = 0;
    pic10f200_step(&cpu);
    CHECK((cpu.ram[PIC10_STATUS] & 0x18u) == 0x18u);
}

static void test_disassembler(void)
{
    char text[64];

    CHECK(pic10_disassemble(0x506, text, sizeof(text)));
    CHECK(strcmp(text, "BSF GPIO,0") == 0);
    CHECK(pic10_disassemble(0xA17, text, sizeof(text)));
    CHECK(strcmp(text, "GOTO 0x017") == 0);
}

static void test_extensible_board_devices(void)
{
    HexImage image = blank_image(false);
    Pic10F200 cpu;
    SimBoard board;
    SimLed led;
    SimButton button;
    int led_net;
    int button_net;

    image.program[0] = 0x506; /* BSF GPIO,0 */
    pic10f200_init(&cpu, &image);
    cpu.tris_gpio = 0x0E;     /* GP0输出，GP1~GP3输入 */

    sim_board_init(&board, &cpu);
    sim_led_init(&led, "测试LED", 255, 0, 0, true);
    sim_button_init(&button, "测试按键", true);
    led_net = sim_board_add_net(&board, "LED_NET");
    button_net = sim_board_add_net(&board, "BUTTON_NET");
    CHECK(sim_board_connect_pic(&board, led_net, 0));
    CHECK(sim_board_connect_device(&board, led_net, &led.base, 0));
    CHECK(sim_board_connect_pic(&board, button_net, 3));
    CHECK(sim_board_connect_device(&board, button_net, &button.base, 0));

    sim_board_step(&board);
    CHECK(led.lit);
    CHECK((pic10f200_gpio_value(&cpu) & (1u << 3)) != 0);

    sim_button_set_pressed(&button, true);
    sim_board_resolve(&board);
    CHECK((pic10f200_gpio_value(&cpu) & (1u << 3)) == 0);
}

int main(void)
{
    test_addwf_flags();
    test_subwf_borrow();
    test_skip_cycles();
    test_call_retlw();
    test_indirect_addressing();
    test_timer0_write_inhibit();
    test_watchdog_wakes_sleep();
    test_external_t0cki_and_pin_wakeup();
    test_status_read_only_bits();
    test_disassembler();
    test_extensible_board_devices();

    if (failures != 0) {
        fprintf(stderr, "CPU单元测试失败：%u项\n", failures);
        return EXIT_FAILURE;
    }
    printf("CPU单元测试通过。\n");
    return EXIT_SUCCESS;
}
