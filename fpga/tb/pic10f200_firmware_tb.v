`timescale 1ns/1ps

/*
 * 使用构建目录中的真实 firmware.mem 测量 GP0 翻转间隔。
 *
 * 为提高仿真速度，这里让每个 FPGA 时钟都产生 cpu_ce。输出的计数单位仍是
 * PIC 指令周期；板级 1 MHz 时基在 clock_enable 的独立测试中验证。
 */
module pic10f200_firmware_tb;
    reg clk = 0;
    reg reset_n = 0;
    wire [7:0] address;
    wire [11:0] instruction;
    wire [3:0] gpio_output;
    wire [3:0] gpio_direction;
    wire [7:0] debug_w;
    wire [7:0] debug_status;
    wire [7:0] debug_pc;

    integer pic_cycles = 0;
    integer last_change = 0;
    integer changes = 0;
    integer interval = 0;
    reg old_gp0 = 0;

    always #1 clk = ~clk;

    pic10f200_program_rom rom (
        .address(address),
        .instruction(instruction)
    );

    pic10f200_core cpu (
        .clk(clk),
        .reset_n(reset_n),
        .cpu_ce(1'b1),
        .program_address(address),
        .program_instruction(instruction),
        .gpio_input(4'b1000),
        .gpio_output(gpio_output),
        .gpio_direction(gpio_direction),
        .debug_w(debug_w),
        .debug_status(debug_status),
        .debug_pc(debug_pc)
    );

    always @(negedge clk) begin
        if (reset_n) begin
            pic_cycles = pic_cycles + 1;
            if (gpio_output[0] != old_gp0) begin
                interval = pic_cycles - last_change;
                $display("GP0=%0d cycle=%0d interval=%0d pc=0x%02x",
                         gpio_output[0], pic_cycles, interval, debug_pc);
                old_gp0 = gpio_output[0];
                last_change = pic_cycles;
                changes = changes + 1;
                if (changes > 1 &&
                    (interval < 999990 || interval > 1000010)) begin
                    $display("FAIL: expected about 1,000,000 PIC cycles");
                    $finish;
                end
                if (changes == 3) begin
                    $display("PASS: firmware toggles GP0 every one second");
                    $finish;
                end
            end
        end
    end

    initial begin
        #5 reset_n = 1;
        #5000000;
        $display("FAIL: firmware did not produce four GP0 changes");
        $finish;
    end
endmodule
