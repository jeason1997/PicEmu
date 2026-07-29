/*
 * button_conditioner.v
 *
 * 异步机械按键输入调理模块：
 *   1. 用两级触发器把异步按键同步到系统时钟域；
 *   2. 只有输入连续保持足够长时间后，才更新稳定状态（消抖）；
 *   3. 每次确认“按下”时只输出一个时钟周期的脉冲。
 *
 * Tang Nano 1K 板载按键为低电平有效，因此接口名称带有 _n。
 * 本模块没有绑定任何具体 FPGA 引脚，可以在后续 PIC10F200 工程中复用。
 */
module button_conditioner #(
    /* 27 MHz × 20 ms = 540000 个时钟周期。 */
    parameter integer STABLE_CYCLES = 540000,
    /* 540000 小于 2^20，20 位计数器足够。 */
    parameter integer COUNTER_WIDTH = 20
) (
    input  wire clk,
    input  wire reset_n,
    input  wire button_n,
    output reg  pressed_pulse,
    output reg  released_pulse,
    output reg  stable_pressed
);

    /* 两级同步器可显著降低异步输入造成亚稳态传播的概率。 */
    reg button_meta_n;
    reg button_sync_n;
    reg [COUNTER_WIDTH-1:0] stable_counter;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            button_meta_n <= 1'b1;
            button_sync_n <= 1'b1;
        end else begin
            button_meta_n <= button_n;
            button_sync_n <= button_meta_n;
        end
    end

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            stable_counter  <= {COUNTER_WIDTH{1'b0}};
            stable_pressed  <= 1'b0;
            pressed_pulse   <= 1'b0;
            released_pulse  <= 1'b0;
        end else begin
            /* 事件脉冲默认拉低，仅在状态确认变化的那个周期置高。 */
            pressed_pulse  <= 1'b0;
            released_pulse <= 1'b0;

            /*
             * stable_pressed=1 表示已经确认按下。
             * button_sync_n 是低有效电平，所以两者相等代表状态没有变化。
             */
            if (button_sync_n == !stable_pressed) begin
                stable_counter <= {COUNTER_WIDTH{1'b0}};
            end else if (stable_counter == STABLE_CYCLES - 1) begin
                stable_counter <= {COUNTER_WIDTH{1'b0}};
                stable_pressed <= !button_sync_n;

                if (!button_sync_n)
                    pressed_pulse <= 1'b1;
                else
                    released_pulse <= 1'b1;
            end else begin
                stable_counter <= stable_counter + 1'b1;
            end
        end
    end

endmodule
