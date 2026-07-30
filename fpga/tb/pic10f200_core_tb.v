`timescale 1ns/1ps

/*
 * 核心冒烟测试程序：
 *   MOVLW 0x0e ; GP0 输出
 *   TRIS  GPIO
 *   MOVLW 0x01
 *   MOVWF GPIO ; GP0=1
 *   MOVLW 0x00
 *   MOVWF GPIO ; GP0=0
 *   GOTO 2
 */
module pic10f200_core_tb;
    reg clk = 0;
    reg reset_n = 0;
    reg cpu_ce = 0;
    reg [11:0] rom [0:255];
    wire [7:0] address;
    wire [11:0] instruction = rom[address];
    wire [3:0] gpio_output;
    wire [3:0] gpio_direction;
    wire [7:0] debug_w;
    wire [7:0] debug_status;
    wire [7:0] debug_pc;

    integer changes = 0;
    reg old_gp0 = 0;
    integer i;

    always #5 clk = ~clk;

    pic10f200_core dut (
        .clk(clk),
        .reset_n(reset_n),
        .cpu_ce(cpu_ce),
        .program_address(address),
        .program_instruction(instruction),
        .gpio_input(4'b1000),
        .gpio_output(gpio_output),
        .gpio_direction(gpio_direction),
        .debug_w(debug_w),
        .debug_status(debug_status),
        .debug_pc(debug_pc)
    );

    always @(posedge clk) begin
        if (reset_n) begin
            cpu_ce <= 1'b1;
            if (gpio_output[0] != old_gp0) begin
                changes = changes + 1;
                old_gp0 = gpio_output[0];
            end
        end
    end

    initial begin
        for (i = 0; i < 256; i = i + 1)
            rom[i] = 12'h000;
        rom[0] = 12'hc0e;
        rom[1] = 12'h006;
        rom[2] = 12'hc01;
        rom[3] = 12'h026;
        rom[4] = 12'hc00;
        rom[5] = 12'h026;
        rom[6] = 12'ha02;

        #30 reset_n = 1;
        #500;

        if (gpio_direction !== 4'b1110) begin
            $display("FAIL: TRIS GPIO=%b", gpio_direction);
            $finish;
        end
        if (changes < 4) begin
            $display("FAIL: GP0 changes=%0d", changes);
            $finish;
        end
        $display("PASS: PIC10F200 fetch/decode/TRIS/GPIO/GOTO");
        $finish;
    end
endmodule
