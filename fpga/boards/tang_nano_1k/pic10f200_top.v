/*
 * Tang Nano 1K 上的 PIC10F200 板级顶层。
 *
 * GP0 -> 红色 LED（pin 9）
 * GP1 -> 蓝色 LED（pin 10）
 * GP2 -> 绿色 LED（pin 11）
 * GP3 <- BTN1（pin 13）
 * BTN2 -> FPGA/PIC 核心复位（pin 44）
 */
module top (
    input  wire       sys_clk,
    input  wire       btn1,
    input  wire       btn2,
    output wire [2:0] led
);
    wire cpu_ce;
    wire gp3_input;
    wire [7:0] program_address;
    wire [11:0] program_instruction;
    wire [3:0] gpio_output;
    wire [3:0] gpio_direction;

    wire [7:0] debug_w_unused;
    wire [7:0] debug_status_unused;
    wire [7:0] debug_pc_unused;

    clock_enable pic_instruction_clock (
        .clk(sys_clk),
        .reset_n(btn2),
        .enable(cpu_ce)
    );

    input_synchronizer gp3_synchronizer (
        .clk(sys_clk),
        .async_input(btn1),
        .sync_output(gp3_input)
    );

    pic10f200_program_rom rom (
        .address(program_address),
        .instruction(program_instruction)
    );

    pic10f200_core cpu (
        .clk(sys_clk),
        .reset_n(btn2),
        .cpu_ce(cpu_ce),
        .program_address(program_address),
        .program_instruction(program_instruction),
        .gpio_input({gp3_input, 3'b000}),
        .gpio_output(gpio_output),
        .gpio_direction(gpio_direction),
        .debug_w(debug_w_unused),
        .debug_status(debug_status_unused),
        .debug_pc(debug_pc_unused)
    );

    /*
     * Tang Nano 1K RGB LED 为低电平点亮。
     * PIC 输出脚显示锁存电平；输入态时让 LED 熄灭，避免悬空发光。
     */
    assign led[0] = gpio_direction[0] ? 1'b1 : ~gpio_output[0];
    assign led[1] = gpio_direction[1] ? 1'b1 : ~gpio_output[1];
    assign led[2] = gpio_direction[2] ? 1'b1 : ~gpio_output[2];
endmodule
