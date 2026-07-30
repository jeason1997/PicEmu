/*
 * PIC10F200 FPGA 核心。
 *
 * 这是提交版本中已经在 Tang Nano 1K 实板验证过的精简数据通路。
 * 它真正从程序 ROM 读取并执行 XC8 的 12 位机器指令，不包含任何
 * blink 或 LED 专用状态机。先用它恢复硬件基线，再逐条扩展指令。
 */
module pic10f200_core (
    input  wire        clk,
    input  wire        reset_n,
    input  wire        cpu_ce,

    output wire [7:0]  program_address,
    input  wire [11:0] program_instruction,

    input  wire [3:0]  gpio_input,
    output wire [3:0]  gpio_output,
    output wire [3:0]  gpio_direction,

    output wire [7:0]  debug_w,
    output wire [7:0]  debug_status,
    output wire [7:0]  debug_pc
);
    localparam STATUS_Z  = 2;
    localparam STATUS_PD = 3;
    localparam STATUS_TO = 4;

    reg [7:0] w;
    reg [7:0] pc;
    reg [7:0] status;
    reg [7:0] osccal;
    reg [7:0] gpr10;
    reg [7:0] gpr11;
    reg [7:0] gpr12;
    reg [3:0] gpio_latch;
    reg [3:0] tris_gpio;
    reg       extra_cycle;

    wire [4:0] file_address = program_instruction[4:0];
    wire       destination_file = program_instruction[5];

    reg [7:0] file_value;
    reg [7:0] decremented_value;
    reg [7:0] logic_value;

    assign program_address = pc;
    assign gpio_output = gpio_latch;
    assign gpio_direction = tris_gpio | 4'b1000;
    assign debug_w = w;
    assign debug_status = status;
    assign debug_pc = pc;

    always @* begin
        case (file_address)
            5'h02: file_value = pc;
            5'h03: file_value = status;
            5'h05: file_value = osccal;
            5'h06: file_value = {
                4'h0,
                ((gpio_latch & ~(tris_gpio | 4'b1000)) |
                 (gpio_input &  (tris_gpio | 4'b1000)))
            };
            5'h10: file_value = gpr10;
            5'h11: file_value = gpr11;
            5'h12: file_value = gpr12;
            default: file_value = 8'h00;
        endcase
    end

    task write_file;
        input [4:0] address;
        input [7:0] value;
        begin
            case (address)
                5'h02: pc <= value;
                5'h03: status <= {3'b000, status[4:3], value[2:0]};
                5'h05: osccal <= value;
                5'h06: gpio_latch <= value[3:0];
                5'h10: gpr10 <= value;
                5'h11: gpr11 <= value;
                5'h12: gpr12 <= value;
                default: begin end
            endcase
        end
    endtask

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            w <= 0;
            pc <= 0;
            status <= (1 << STATUS_TO) | (1 << STATUS_PD);
            osccal <= 0;
            gpio_latch <= 0;
            tris_gpio <= 4'b1111;
            extra_cycle <= 0;
            decremented_value <= 0;
            logic_value <= 0;
            gpr10 <= 0;
            gpr11 <= 0;
            gpr12 <= 0;
        end else if (cpu_ce) begin
            if (extra_cycle) begin
                /*
                 * 跳转和成功跳过后的流水线空泡。PC 已经指向新地址，
                 * 本周期只等待同步 BSRAM 准备好对应指令。
                 */
                extra_cycle <= 1'b0;
            end else begin
                pc <= pc + 1'b1;

                if (program_instruction == 12'h000) begin
                    /* NOP */
                end else if ((program_instruction & 12'hff8) == 12'h000 &&
                             program_instruction[2:0] == 3'd6) begin
                    /* TRIS GPIO */
                    tris_gpio <= w[3:0] | 4'b1000;
                end else if ((program_instruction & 12'hfe0) == 12'h020) begin
                    /* MOVWF f */
                    write_file(file_address, w);
                end else if ((program_instruction & 12'hfe0) == 12'h060) begin
                    /* CLRF f */
                    write_file(file_address, 8'h00);
                    status[STATUS_Z] <= 1'b1;
                end else if ((program_instruction & 12'hfc0) == 12'h180) begin
                    /* XORWF f,d */
                    logic_value = file_value ^ w;
                    if (destination_file)
                        write_file(file_address, logic_value);
                    else
                        w <= logic_value;
                    status[STATUS_Z] <= (logic_value == 0);
                end else if ((program_instruction & 12'hfc0) == 12'h200) begin
                    /* MOVF f,d */
                    if (destination_file)
                        write_file(file_address, file_value);
                    else
                        w <= file_value;
                    status[STATUS_Z] <= (file_value == 0);
                end else if ((program_instruction & 12'hfc0) == 12'h2c0) begin
                    /* DECFSZ f,d */
                    decremented_value = file_value - 1'b1;
                    if (destination_file)
                        write_file(file_address, decremented_value);
                    else
                        w <= decremented_value;

                    if (decremented_value == 0) begin
                        pc <= pc + 2'd2;
                        extra_cycle <= 1'b1;
                    end
                end else if ((program_instruction & 12'hf00) == 12'h600) begin
                    /* BTFSC f,b：指定位为0时跳过下一条指令。 */
                    if (!file_value[program_instruction[7:5]]) begin
                        pc <= pc + 2'd2;
                        extra_cycle <= 1'b1;
                    end
                end else if ((program_instruction & 12'he00) == 12'ha00) begin
                    /* GOTO k */
                    pc <= program_instruction[7:0];
                    extra_cycle <= 1'b1;
                end else if ((program_instruction & 12'hf00) == 12'hc00) begin
                    /* MOVLW k */
                    w <= program_instruction[7:0];
                end
            end
        end
    end
endmodule
