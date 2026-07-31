#include "picemu/core/disassembler.h"
#include "picemu/core/pic10_cpu.h"
#include "picemu/sim/board.h"
#include "picemu/sim/mcu/pic10.h"
#include "picemu/sim/device.h"
#include "picemu/sim/devices/led.h"
#include "picemu/sim/devices/hc595.h"
#include "picemu/sim/devices/max7219.h"
#include "picemu/sim/devices/led_matrix_8x8.h"
#include "picemu/sim/devices/seven_segment.h"
#include "picemu/sim/devices/button.h"
#include "picemu/sim/devices/buzzer.h"
#include "picemu/sim/devices/w25q.h"
#include "picemu/sim/circuit_config.h"

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

static void w25_send_byte(SimW25q *flash, uint8_t value)
{
    unsigned bit;
    for (bit = 0; bit < 8; ++bit) {
        bool high = (value & (0x80u >> bit)) != 0;
        sim_w25q_set_lines(flash, false, false, high);
        sim_w25q_set_lines(flash, false, true, high);
        sim_w25q_set_lines(flash, false, false, high);
    }
}

static uint8_t w25_receive_byte(SimW25q *flash)
{
    uint8_t value = 0;
    unsigned bit;
    for (bit = 0; bit < 8; ++bit) {
        sim_w25q_set_lines(flash, false, true, false);
        value = (uint8_t)((value << 1) |
                          (sim_w25q_miso(flash) ? 1u : 0u));
        sim_w25q_set_lines(flash, false, false, false);
    }
    return value;
}

static void test_w25q_spi_read(void)
{
    static const uint8_t initial[] = {0x50, 0x49, 0x43, 0x45};
    SimW25q flash;
    uint8_t copy[4];

    CHECK(sim_w25q_init(&flash, 128u * 1024u,
                        initial, sizeof(initial)));
    sim_w25q_set_lines(&flash, false, false, false);
    w25_send_byte(&flash, 0x03);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0x00);
    CHECK(w25_receive_byte(&flash) == 0x50);
    CHECK(w25_receive_byte(&flash) == 0x49);
    CHECK(w25_receive_byte(&flash) == 0x43);
    CHECK(w25_receive_byte(&flash) == 0x45);
    sim_w25q_set_lines(&flash, true, false, false);

    CHECK(sim_w25q_read(&flash, 0, copy, sizeof(copy)));
    CHECK(memcmp(copy, initial, sizeof(copy)) == 0);
    CHECK(!sim_w25q_read(&flash, flash.capacity - 1,
                         copy, sizeof(copy)));
    sim_w25q_destroy(&flash);
}

static void test_w25q_spi_program(void)
{
    SimW25q flash;
    uint8_t value = 0;

    CHECK(sim_w25q_init(&flash, 128u * 1024u, NULL, 0));

    /* 没有先发送写使能时，页编程必须被忽略。 */
    sim_w25q_set_lines(&flash, false, false, false);
    w25_send_byte(&flash, 0x02);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0x01);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0xA5);
    sim_w25q_set_lines(&flash, true, false, false);
    CHECK(sim_w25q_read(&flash, 0x100, &value, 1));
    CHECK(value == 0xFF);

    sim_w25q_set_lines(&flash, false, false, false);
    w25_send_byte(&flash, 0x06);
    sim_w25q_set_lines(&flash, true, false, false);

    sim_w25q_set_lines(&flash, false, false, false);
    w25_send_byte(&flash, 0x02);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0x01);
    w25_send_byte(&flash, 0x00);
    w25_send_byte(&flash, 0xA5);
    sim_w25q_set_lines(&flash, true, false, false);
    CHECK(sim_w25q_read(&flash, 0x100, &value, 1));
    CHECK(value == 0xA5);
    CHECK(!flash.write_enable);

    value = 0x5A;
    CHECK(sim_w25q_write(&flash, 0x100, &value, 1));
    value = 0;
    CHECK(sim_w25q_read(&flash, 0x100, &value, 1));
    CHECK(value == 0x5A);

    sim_w25q_destroy(&flash);
}

static HexImage blank_image(bool watchdog_enabled)
{
    HexImage image;
    unsigned i;

    memset(&image, 0, sizeof(image));
    for (i = 0; i < PIC10_MAX_PROGRAM_WORDS; ++i) {
        image.program[i] = 0x000; /* NOP */
    }
    image.config_present = true;
    image.config_word = watchdog_enabled ? 0x0FFFu : 0x0FFBu;
    return image;
}

static void test_addwf_flags(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    /* ADDWF 0x10,F */
    image.program[0] = 0x1C0u | 0x20u | 0x10u;
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.w = 0x01;
    cpu.ram[0x10] = 0xFF;
    pic10_step(&cpu);

    CHECK(cpu.ram[0x10] == 0x00);
    CHECK((cpu.ram[PIC10_STATUS] & 0x01u) != 0); /* C */
    CHECK((cpu.ram[PIC10_STATUS] & 0x02u) != 0); /* DC */
    CHECK((cpu.ram[PIC10_STATUS] & 0x04u) != 0); /* Z */
}

static void test_subwf_borrow(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    /* SUBWF 0x10,W：0x01 - 0x02 = 0xFF，产生借位时C清零。 */
    image.program[0] = 0x080u | 0x10u;
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.w = 0x02;
    cpu.ram[0x10] = 0x01;
    pic10_step(&cpu);

    CHECK(cpu.w == 0xFF);
    CHECK((cpu.ram[PIC10_STATUS] & 0x01u) == 0);
    CHECK((cpu.ram[PIC10_STATUS] & 0x04u) == 0);
}

static void test_skip_cycles(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;
    Pic10StepResult result;

    /* BTFSC 0x10,0：bit为0时跳过下一条。 */
    image.program[0] = 0x600u | 0x10u;
    image.program[1] = 0xCAA; /* MOVLW 0xAA，不应执行 */
    image.program[2] = 0xC55; /* MOVLW 0x55 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);

    result = pic10_step(&cpu);
    CHECK(result.instruction_cycles == 2);
    CHECK(cpu.pc == 2);
    pic10_step(&cpu);
    CHECK(cpu.w == 0x55);
}

static void test_call_retlw(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    image.program[0] = 0x905; /* CALL 0x05 */
    image.program[1] = 0xC42; /* 返回后执行 */
    image.program[5] = 0x899; /* RETLW 0x99 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);

    pic10_step(&cpu);
    CHECK(cpu.pc == 5);
    pic10_step(&cpu);
    CHECK(cpu.pc == 1);
    CHECK(cpu.w == 0x99);
    pic10_step(&cpu);
    CHECK(cpu.w == 0x42);
}

static void test_indirect_addressing(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    /* MOVWF INDF，FSR指向0x10。 */
    image.program[0] = 0x020;
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.ram[PIC10_FSR] = 0x10;
    cpu.w = 0xA5;
    pic10_step(&cpu);

    CHECK(cpu.ram[0x10] == 0xA5);
    CHECK(pic10_read_register(&cpu, PIC10_INDF) == 0xA5);
}

static void test_pic10f202_memory_map(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    /* PIC10F202的GOTO可以使用第9个目标位，跳到0x1AA。 */
    image.program[0] = 0xA00u | 0x1AAu;
    image.program[0x1AA] = 0xC55u; /* MOVLW 0x55 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F202);

    pic10_step(&cpu);
    CHECK(cpu.pc == 0x1AA);
    pic10_step(&cpu);
    CHECK(cpu.w == 0x55);

    /* 10F202从0x08开始有24字节GPR；10F200的0x08未实现。 */
    image.program[0] = 0x020u; /* MOVWF INDF */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F202);
    cpu.ram[PIC10_FSR] = 0x08;
    cpu.w = 0xA5;
    pic10_step(&cpu);
    CHECK(pic10_read_register(&cpu, 0x08) == 0xA5);

    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.ram[PIC10_FSR] = 0x08;
    cpu.w = 0xA5;
    pic10_step(&cpu);
    CHECK(pic10_read_register(&cpu, 0x08) == 0x00);
}

static void test_timer0_write_inhibit(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    image.program[0] = 0x020 | PIC10_TMR0; /* MOVWF TMR0 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.option = 0x08; /* 内部时钟，预分频器给WDT，TMR0每周期+1 */
    cpu.w = 10;

    pic10_step(&cpu); /* 写入周期 */
    pic10_step(&cpu); /* 抑制1 */
    pic10_step(&cpu); /* 抑制2 */
    CHECK(cpu.ram[PIC10_TMR0] == 10);
    pic10_step(&cpu);
    CHECK(cpu.ram[PIC10_TMR0] == 11);
}

static void test_watchdog_wakes_sleep(void)
{
    HexImage image = blank_image(true);
    Pic10Cpu cpu;

    image.program[0] = 0x003; /* SLEEP */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.watchdog_base_period = 2; /* 测试中缩短标称18ms超时 */
    cpu.option = 0x00;            /* PSA给TMR0，WDT为1:1 */

    pic10_step(&cpu);
    CHECK(cpu.sleeping);
    pic10_step(&cpu);
    CHECK(!cpu.sleeping);
    CHECK((cpu.ram[PIC10_STATUS] & (1u << PIC10_STATUS_TO)) == 0);
    CHECK((cpu.ram[PIC10_STATUS] & (1u << PIC10_STATUS_PD)) == 0);
}

static void test_external_t0cki_and_pin_wakeup(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.option = 0x68; /* T0CS=1、上升沿、PSA=1、弱上拉关闭 */
    cpu.tris_gpio = 0x0F;
    pic10_drive_pin(&cpu, 2, true, false);
    pic10_drive_pin(&cpu, 2, true, true);
    CHECK(cpu.ram[PIC10_TMR0] == 1);

    cpu.sleeping = true;
    cpu.option = 0x40; /* GPWU=0，允许引脚变化唤醒 */
    pic10_drive_pin(&cpu, 3, true, false);
    pic10_drive_pin(&cpu, 3, true, true);
    CHECK(!cpu.sleeping);
}

static void test_status_read_only_bits(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;

    /* MOVWF STATUS不应清除只读的TO和PD。 */
    image.program[0] = 0x020u | PIC10_STATUS;
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.w = 0;
    pic10_step(&cpu);
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
    Pic10Cpu cpu;
    SimBoard board;
    SimPic10Mcu adapter;
    SimLed led;
    SimButton button;
    int led_net;
    int button_net;

    image.program[0] = 0x506; /* BSF GPIO,0 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.tris_gpio = 0x0E;     /* GP0输出，GP1~GP3输入 */

    sim_pic10_mcu_init(&adapter, &cpu, 4000000u);
    sim_board_init(&board, &adapter.base);
    sim_led_init(&led, "测试LED", 255, 0, 0, true);
    sim_button_init(&button, "测试按键", true);
    led_net = sim_board_add_net(&board, "LED_NET");
    button_net = sim_board_add_net(&board, "BUTTON_NET");
    CHECK(sim_board_connect_mcu(&board, led_net, 0));
    CHECK(sim_board_connect_device(&board, led_net, &led.base, 0));
    CHECK(sim_board_connect_mcu(&board, button_net, 3));
    CHECK(sim_board_connect_device(&board, button_net, &button.base, 0));

    sim_board_step(&board);
    CHECK(led.lit);
    CHECK((pic10_gpio_value(&cpu) & (1u << 3)) != 0);

    sim_button_set_pressed(&button, true);
    sim_board_resolve(&board);
    CHECK((pic10_gpio_value(&cpu) & (1u << 3)) == 0);
}

static void test_passive_buzzer_frequency(void)
{
    SimBuzzer buzzer;

    sim_buzzer_init(&buzzer, "test buzzer");
    buzzer.base.ops->pin_changed(&buzzer.base, 0, SIM_LEVEL_HIGH);
    buzzer.base.ops->tick(&buzzer.base, 500, 1000000u);
    buzzer.base.ops->pin_changed(&buzzer.base, 0, SIM_LEVEL_LOW);

    /* 1MHz周期下，500周期半波对应1000Hz。 */
    CHECK(buzzer.frequency_hz > 999.0 && buzzer.frequency_hz < 1001.0);
    CHECK(buzzer.half_period_cycles == 500);
}

static void test_led_pwm_brightness(void)
{
    SimLed led;
    unsigned i;

    sim_led_init(&led, "PWM LED", 255, 0, 0, true);

    /* 1000周期采样窗内，高低电平各占一半，亮度应接近50%。 */
    for (i = 0; i < 10; ++i) {
        led.base.ops->pin_changed(&led.base, 0, SIM_LEVEL_HIGH);
        led.base.ops->tick(&led.base, 50, 60000u);
        led.base.ops->pin_changed(&led.base, 0, SIM_LEVEL_LOW);
        led.base.ops->tick(&led.base, 50, 60000u);
    }
    CHECK(led.brightness >= 126 && led.brightness <= 128);

    led.base.ops->reset(&led.base);
    CHECK(!led.lit);
    CHECK(led.brightness == 0);
}

static void test_seven_segment_shift_and_latch(void)
{
    SimHc595 chip;
    SimSevenSegment display;
    SimDevice *shift_device;
    uint8_t value = 0x6Du; /* 数字 5：a、f、g、c、d 段点亮。 */
    unsigned bit;

    sim_hc595_init(&chip, "test shift register");
    sim_seven_segment_init(&display, "test display", true);
    shift_device = &chip.base;
    for (bit = 0; bit < 8; ++bit) {
        SimLevel data = (value & (0x80u >> bit))
            ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
        shift_device->ops->pin_changed(shift_device, 0, data);
        shift_device->ops->pin_changed(shift_device, 1, SIM_LEVEL_HIGH);
        shift_device->ops->pin_changed(shift_device, 1, SIM_LEVEL_LOW);
    }
    CHECK(chip.outputs == 0);
    shift_device->ops->pin_changed(shift_device, 2, SIM_LEVEL_HIGH);
    CHECK(chip.outputs == value);
    for (bit = 0; bit < 8; ++bit) {
        display.base.ops->pin_changed(
            &display.base, bit, (chip.outputs & (1u << bit))
            ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW);
    }
    CHECK(sim_seven_segment_visible_segments(&display) == value);
    display.base.ops->reset(&display.base);
    CHECK(sim_seven_segment_visible_segments(&display) == 0);
}

static void max7219_send(SimMax7219 *chip, uint8_t address, uint8_t data)
{
    unsigned bit;
    SimDevice *device = &chip->base;
    device->observed[SIM_MAX7219_LOAD] = SIM_LEVEL_LOW;
    device->ops->pin_changed(device, SIM_MAX7219_LOAD, SIM_LEVEL_LOW);
    for (bit = 0; bit < 16; ++bit) {
        bool one = (((uint16_t)address << 8 | data) &
                    (0x8000u >> bit)) != 0;
        device->observed[SIM_MAX7219_DIN] = one
            ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
        device->observed[SIM_MAX7219_CLK] = SIM_LEVEL_HIGH;
        device->ops->pin_changed(device, SIM_MAX7219_CLK, SIM_LEVEL_HIGH);
        device->observed[SIM_MAX7219_CLK] = SIM_LEVEL_LOW;
        device->ops->pin_changed(device, SIM_MAX7219_CLK, SIM_LEVEL_LOW);
    }
    device->observed[SIM_MAX7219_LOAD] = SIM_LEVEL_HIGH;
    device->ops->pin_changed(device, SIM_MAX7219_LOAD, SIM_LEVEL_HIGH);
}

static void test_max7219_and_led_matrix(void)
{
    SimMax7219 chip;
    SimLedMatrix8x8 matrix;
    unsigned pin;

    sim_max7219_init(&chip, "MAX7219");
    sim_led_matrix_8x8_init(&matrix, "8x8 matrix", true);
    max7219_send(&chip, 0x0C, 1);       /* 退出关断模式。 */
    max7219_send(&chip, 1, 0x81);
    CHECK(sim_max7219_visible_row(&chip, 0) == 0x81);

    for (pin = 0; pin < 8; ++pin) {
        matrix.base.observed[pin] = chip.base.drive[SIM_MAX7219_SEG0 + pin];
        matrix.base.observed[8 + pin] = chip.base.drive[SIM_MAX7219_DIG0 + pin];
    }
    matrix.base.ops->pin_changed(&matrix.base, 0,
                                 matrix.base.observed[0]);
    CHECK(sim_led_matrix_8x8_row(&matrix, 0) == 0x81);

    max7219_send(&chip, 0x0F, 1);
    CHECK(sim_max7219_visible_row(&chip, 3) == 0xFF);
    max7219_send(&chip, 0x0C, 0);
    CHECK(sim_max7219_visible_row(&chip, 0) == 0);
}

static void test_buzzer_uses_configured_clock(void)
{
    SimBuzzer buzzer;

    sim_buzzer_init(&buzzer, "clocked buzzer");
    buzzer.base.ops->pin_changed(&buzzer.base, 0, SIM_LEVEL_HIGH);
    buzzer.base.ops->tick(&buzzer.base, 1000, 2000000u);
    buzzer.base.ops->pin_changed(&buzzer.base, 0, SIM_LEVEL_LOW);
    CHECK(buzzer.frequency_hz > 999.0 && buzzer.frequency_hz < 1001.0);
}

static void test_network_merge(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;
    SimPic10Mcu adapter;
    SimBoard board;
    SimLed led_a;
    SimLed led_b;
    int first;
    int second;

    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    sim_pic10_mcu_init(&adapter, &cpu, 4000000u);
    sim_board_init(&board, &adapter.base);
    sim_led_init(&led_a, "A", 255, 0, 0, true);
    sim_led_init(&led_b, "B", 0, 255, 0, true);
    first = sim_board_add_net(&board, "first");
    second = sim_board_add_net(&board, "second");
    CHECK(sim_board_connect_mcu(&board, first, 0));
    CHECK(sim_board_connect_device(&board, first, &led_a.base, 0));
    CHECK(sim_board_connect_device(&board, second, &led_b.base, 0));
    CHECK(sim_board_merge_nets(&board, first, second));
    CHECK(board.nets[first].endpoint_count == 3);
    CHECK(board.nets[second].endpoint_count == 0);
}

static void test_generic_circuit_properties(void)
{
    CircuitConfig config;
    char error[256];
    const CircuitPartConfig *button;

    CHECK(circuit_config_load("tests/fixtures/circuit_config.json", &config,
                              error, sizeof(error)));
    CHECK(config.clock_hz == 8000000u);
    CHECK(config.part_count == 1);
    button = &config.parts[0];
    CHECK(!circuit_part_get_bool(button, "activeLow", true));
    CHECK(strcmp(circuit_part_get(button, "label", ""), "TEST") == 0);
    CHECK(circuit_part_get_long(button, "debounceMs", 0) == 25);
    CHECK(circuit_part_get_long(button, "missing", 123) == 123);
}

static void test_fast_countdown_loop(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu cpu;
    unsigned cycles;

    image.program[0] = 0x2F0u; /* DECFSZ 0x10,F */
    image.program[1] = 0xA00u; /* GOTO 0 */
    pic10_init(&cpu, &image, &PIC_DEVICE_PIC10F200);
    cpu.ram[0x10] = 5;

    cycles = pic10_step_cycles(&cpu);
    CHECK(cycles == 14u); /* 3 * 5 - 1 */
    CHECK(cpu.cycles == 14u);
    CHECK(cpu.pc == 2u);
    CHECK(cpu.ram[0x10] == 0);
}

static void test_fast_xc8_postdecrement_loop(void)
{
    HexImage image = blank_image(false);
    Pic10Cpu fast_cpu;
    Pic10Cpu reference_cpu;
    unsigned fast_cycles;

    image.program[0] = 0xC01u; /* MOVLW 1 */
    image.program[1] = 0x0B0u; /* SUBWF 0x10,F */
    image.program[2] = 0x290u; /* INCF 0x10,W */
    image.program[3] = 0x643u; /* BTFSC STATUS,Z */
    image.program[4] = 0xA08u; /* GOTO 8 */
    image.program[5] = 0x000u; /* NOP */
    image.program[6] = 0xA00u; /* GOTO 0 */
    image.program[8] = 0x000u; /* exit */

    pic10_init(&fast_cpu, &image, &PIC_DEVICE_PIC10F200);
    pic10_init(&reference_cpu, &image, &PIC_DEVICE_PIC10F200);
    fast_cpu.ram[0x10] = 5;
    reference_cpu.ram[0x10] = 5;

    fast_cycles = pic10_step_cycles(&fast_cpu);
    while (reference_cpu.pc != 8u) {
        pic10_step(&reference_cpu);
    }

    CHECK(fast_cycles == 46u); /* 8 * 5 + 6 */
    CHECK(fast_cpu.cycles == reference_cpu.cycles);
    CHECK(fast_cpu.pc == reference_cpu.pc);
    CHECK(fast_cpu.w == reference_cpu.w);
    CHECK(fast_cpu.ram[0x10] == reference_cpu.ram[0x10]);
    CHECK(fast_cpu.ram[PIC10_STATUS] ==
          reference_cpu.ram[PIC10_STATUS]);
}

int main(void)
{
    test_addwf_flags();
    test_subwf_borrow();
    test_skip_cycles();
    test_call_retlw();
    test_indirect_addressing();
    test_pic10f202_memory_map();
    test_timer0_write_inhibit();
    test_watchdog_wakes_sleep();
    test_external_t0cki_and_pin_wakeup();
    test_status_read_only_bits();
    test_disassembler();
    test_extensible_board_devices();
    test_passive_buzzer_frequency();
    test_led_pwm_brightness();
    test_seven_segment_shift_and_latch();
    test_max7219_and_led_matrix();
    test_buzzer_uses_configured_clock();
    test_network_merge();
    test_generic_circuit_properties();
    test_fast_countdown_loop();
    test_fast_xc8_postdecrement_loop();
    test_w25q_spi_read();
    test_w25q_spi_program();

    if (failures != 0) {
        fprintf(stderr, "CPU单元测试失败：%u项\n", failures);
        return EXIT_FAILURE;
    }
    printf("CPU单元测试通过。\n");
    return EXIT_SUCCESS;
}
