/*
 * Tang Nano 1K 按键切换 LED 示例
 *
 * BTN1：每确认一次按下，切换 LED1 的亮灭状态。
 * BTN2：复位，按下后 LED1 熄灭。
 *
 * 板载 LED 是低电平点亮，因此 led[0]=0 表示 LED1 亮。
 * 其余两颗 LED 暂时保持熄灭，为以后映射 PIC10F200 的 GP1、GP2 预留。
 */
module top (
    input  wire       sys_clk,
    input  wire       btn1,
    input  wire       btn2,
    output wire [2:0] led
);

    wire button_pressed;
    wire button_released_unused;
    wire button_state_unused;
    reg  led1_on;

    button_conditioner #(
        .STABLE_CYCLES(540000),
        .COUNTER_WIDTH(20)
    ) user_button (
        .clk(sys_clk),
        .reset_n(btn2),
        .button_n(btn1),
        .pressed_pulse(button_pressed),
        .released_pulse(button_released_unused),
        .stable_pressed(button_state_unused)
    );

    always @(posedge sys_clk or negedge btn2) begin
        if (!btn2)
            led1_on <= 1'b0;
        else if (button_pressed)
            led1_on <= ~led1_on;
    end

    /*
     * led[0] 对应物理 9 脚，是这里所称的 LED1。
     * 低电平点亮，所以需要对内部“亮”状态取反。
     */
    assign led[0] = ~led1_on;
    assign led[1] = 1'b1;
    assign led[2] = 1'b1;

endmodule
