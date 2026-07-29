`timescale 1ns/1ps

/*
 * 快速逻辑测试。
 *
 * 为避免仿真 54 万个周期，这里直接测试同一个按键模块的小计数参数；
 * FPGA 综合时顶层仍使用真实的 20 ms 参数。
 */
module top_tb;
    reg clk;
    reg reset_n;
    reg button_n;
    wire pressed_pulse;
    wire released_pulse;
    wire stable_pressed;

    integer press_count;

    button_conditioner #(
        .STABLE_CYCLES(4),
        .COUNTER_WIDTH(3)
    ) dut (
        .clk(clk),
        .reset_n(reset_n),
        .button_n(button_n),
        .pressed_pulse(pressed_pulse),
        .released_pulse(released_pulse),
        .stable_pressed(stable_pressed)
    );

    always #5 clk = ~clk;

    always @(posedge clk) begin
        if (pressed_pulse)
            press_count = press_count + 1;
    end

    initial begin
        clk = 1'b0;
        reset_n = 1'b0;
        button_n = 1'b1;
        press_count = 0;

        #30 reset_n = 1'b1;

        /* 短暂毛刺：不足 4 个稳定周期，不应产生按下事件。 */
        #20 button_n = 1'b0;
        #20 button_n = 1'b1;
        #100;
        if (press_count != 0) begin
            $display("FAIL: 短毛刺被错误识别为按键");
            $finish;
        end

        /* 正常按下并保持，应只产生一次按下事件。 */
        button_n = 1'b0;
        #120;
        if (press_count != 1 || stable_pressed != 1'b1) begin
            $display("FAIL: 正常按下未被正确识别");
            $finish;
        end

        /* 释放后再次按下，应累计第二次事件。 */
        button_n = 1'b1;
        #120;
        button_n = 1'b0;
        #120;
        if (press_count != 2) begin
            $display("FAIL: 第二次按下未被正确识别");
            $finish;
        end

        $display("PASS: 按键同步、消抖和单次事件逻辑正确");
        $finish;
    end
endmodule
