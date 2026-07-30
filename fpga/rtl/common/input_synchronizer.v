/*
 * 单比特异步输入同步器。
 * 两级触发器只解决时钟域亚稳态，不对 PIC 的输入做额外消抖，
 * 以便 FPGA 引脚行为尽量接近真实芯片。
 */
module input_synchronizer (
    input  wire clk,
    input  wire async_input,
    output reg  sync_output
);
    reg meta;

    always @(posedge clk) begin
        meta <= async_input;
        sync_output <= meta;
    end
endmodule
