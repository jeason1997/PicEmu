/*
 * PIC10F200 片上程序存储器：64 个 12 位字。
 *
 * firmware.mem 由构建脚本根据 XC8 生成的 Intel HEX 自动产生。固件数据会
 * 作为 FPGA 位流的一部分写入 GW1NZ-1 的内部配置 Flash。FPGA 上电配置时，
 * 配置电路直接把这些初值装入片上程序 RAM；配置完成后，PIC CPU 只从 RAM
 * 取指，不需要访问外置 Flash，也不会使用组合逻辑实现程序 ROM。
 *
 * 同步读适合 Gowin BSRAM 的硬件结构。PIC 每 27 个 FPGA 时钟才执行一个
 * 指令周期，因此地址变化后有充足时间取得下一条指令。
 */
module pic10f200_program_memory #(
    parameter INIT_FILE = "firmware.mem"
) (
    input  wire        clk,
    input  wire [7:0]  address,
    output reg  [11:0] instruction = 12'h000
);
    (* ram_style = "block" *) reg [11:0] memory [0:63];

    initial begin
        $readmemh(INIT_FILE, memory);
    end

    always @(posedge clk) begin
        if (address[7:6] == 0)
            instruction <= memory[address[5:0]];
        else
            instruction <= 12'h000;
    end
endmodule
