/* PIC10F200 的 256×12 位同步程序 BSRAM，初始化内容随位流写入配置 Flash。 */
module pic10f200_program_memory #(
    parameter INIT_FILE = "firmware.mem"
) (
    input wire clk,
    input wire [7:0] address,
    output reg [11:0] instruction = 12'h000
);
    (* ram_style = "block" *) reg [11:0] memory [0:255];

    initial begin
        /*
         * hex_to_mem.py 固定输出完整的 256 个程序字，未使用位置已经填充 NOP。
         * 这里不能再用 for 循环预清零：Icarus 会按语句顺序执行，但 Yosys 的
         * 存储器初始化归并可能让循环赋值覆盖 readmemh，生成全零 BSRAM。
         */
        $readmemh(INIT_FILE, memory);
    end
    always @(posedge clk) instruction <= memory[address];
endmodule
