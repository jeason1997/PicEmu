/*
 * 27 MHz 到 1 MHz 的精确时钟使能。
 *
 * PIC10F200 使用 4 MHz 振荡器时，每条单周期指令耗时 1 us。Tang Nano 1K
 * 的 27 MHz 时钟恰好可以每 27 拍产生一次使能，因此只需一个 5 位计数器，
 * 比通用分数累加器节省很多 Nano 1K 逻辑资源。
 */
module clock_enable (
    input  wire clk,
    input  wire reset_n,
    output reg  enable
);
    reg [4:0] divider;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            divider <= 0;
            enable <= 1'b0;
        end else if (divider == 5'd26) begin
            divider <= 0;
            enable <= 1'b1;
        end else begin
            divider <= divider + 1'b1;
            enable <= 1'b0;
        end
    end
endmodule
