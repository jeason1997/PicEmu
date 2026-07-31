/* PIC10F200 的 256×12 位同步程序 BSRAM，初始化内容随位流写入配置 Flash。 */
module pic10f200_program_memory #(
    parameter INIT_FILE = "firmware.mem"
) (
    input wire clk,
    input wire [7:0] address,
    output reg [11:0] instruction = 12'h000
);
    (* ram_style = "block" *) reg [11:0] memory [0:255];
    integer i;
    initial begin
        /* 短测试镜像未覆盖的地址必须表现为擦除后的 NOP，而不能传播 X。 */
        for (i = 0; i < 256; i = i + 1)
            memory[i] = 12'h000;
        $readmemh(INIT_FILE, memory);
    end
    always @(posedge clk) instruction <= memory[address];
endmodule
