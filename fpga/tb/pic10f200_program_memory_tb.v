`timescale 1ns/1ps

/*
 * 程序存储器接口测试。
 *
 * 该测试使用短夹具验证已提供地址的同步取指，不读取夹具范围之外的地址。
 * 正式 hex_to_mem.py 固定生成完整 256 字镜像，完整地址范围由真实固件测试覆盖。
 * BSRAM 的实际映射由 FPGA 综合资源报告另外验证。
 */
module pic10f200_program_memory_tb;
    reg clk = 0;
    reg [7:0] address = 0;
    wire [11:0] instruction;

    always #5 clk = ~clk;

    pic10f200_program_memory #(
        .INIT_FILE("fpga/tb/fixtures/program_memory.mem")
    ) dut (
        .clk(clk),
        .address(address),
        .instruction(instruction)
    );

    task check_word;
        input [7:0] expected_address;
        input [11:0] expected_instruction;
        begin
            address = expected_address;
            @(posedge clk);
            #1;
            if (instruction !== expected_instruction) begin
                $display(
                    "FAIL: address=%0d expected=%03x actual=%03x",
                    expected_address,
                    expected_instruction,
                    instruction
                );
                $finish_and_return(1);
            end
        end
    endtask

    initial begin
        check_word(0, 12'h123);
        check_word(1, 12'habc);
        check_word(2, 12'h000);
        check_word(3, 12'hfff);
        $display("PASS: synchronous PIC program memory");
        $finish;
    end
endmodule
