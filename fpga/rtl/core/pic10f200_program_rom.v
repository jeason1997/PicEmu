/*
 * PIC10F200 程序存储器：256 个 12 位字。
 *
 * firmware.mem 由构建脚本根据 XC8 生成的 Intel HEX 自动产生。
 * 使用独立 ROM 模块，后续增加 PIC10F202 时只需扩展地址宽度和深度。
 */
module pic10f200_program_rom (
    input  wire [7:0]  address,
    output wire [11:0] instruction
);
    /*
     * Nano 1K 没有 Gowin BSRAM。当前 1 秒 blink 固件占 38 个程序字，
     * 因此使用 64×12 的组合 ROM；PC 超出该窗口时读作 NOP。
     */
    reg [11:0] memory [0:63];

    initial begin
        $readmemh("firmware.mem", memory);
    end

    assign instruction = (address[7:6] == 0)
        ? memory[address[5:0]] : 12'h000;
endmodule
