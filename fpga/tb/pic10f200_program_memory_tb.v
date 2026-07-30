`timescale 1ns/1ps

/*
 * 程序存储器接口测试。
 *
 * 该测试只验证同步取指和地址越界行为，不依赖 blink 等具体应用固件。
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
        check_word(64, 12'h000);
        $display("PASS: synchronous PIC program memory");
        $finish;
    end
endmodule
