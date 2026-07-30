/*
 * PIC10F200 FPGA 核心（第一阶段）
 *
 * 这一版不是“用 Verilog 写死闪灯”，而是真正从程序 ROM 取出 XC8 编译的
 * 12 位机器指令并执行。为了先在只有 1152 个逻辑单元的 Tang Nano 1K 上
 * 建立可运行的硬件架构，目前实现 XC8 blink 固件实际使用的指令子集：
 *
 *   NOP、TRIS、MOVWF、CLRF、DECFSZ、GOTO、MOVLW
 *
 * 寄存器文件、W、PC、STATUS、GPIO 锁存器和 TRIS 都是真实运行状态。
 * 后续指令会围绕共享 ALU/写回通路逐步加入，避免把软件模拟器的大型
 * if/else 译码直接铺成数千个并行 LUT。
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
    /*
     * 第一阶段物理实现 XC8 一秒 blink 使用的三个 GPR。地址译码仍使用
     * PIC 的 0x10~0x12 地址，后续扩充不会改变固件格式。
     */
    reg [7:0] gpr10;
    reg [7:0] gpr11;
    reg [7:0] gpr12;
    reg [3:0] gpio_latch;
    reg [3:0] tris_gpio;

    /* 跳转和成功跳过需要额外消耗一个 PIC 指令周期。 */
    reg extra_cycle;

    wire [4:0] file_address = program_instruction[4:0];
    wire       destination_file = program_instruction[5];

    reg [7:0] file_value;
    reg [7:0] decremented_value;
    assign program_address = pc;
    assign gpio_output = gpio_latch;
    assign gpio_direction = tris_gpio | 4'b1000;
    assign debug_w = w;
    assign debug_status = status;
    assign debug_pc = pc;

    /*
     * 当前固件会访问 OSCCAL(5)、GPIO(6) 和 GPR 0x10~0x1f。
     * GPIO 读取遵守“输出读锁存器、输入读引脚”的 PIC 行为。
     */
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

    /*
     * 所有文件寄存器写入共用这一条写回通路。未实现的 SFR 写入被忽略，
     * 不会误写到普通 RAM。
     */
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
            gpr10 <= 0;
            gpr11 <= 0;
            gpr12 <= 0;
        end else if (cpu_ce) begin
            if (extra_cycle) begin
                /*
                 * 流水线空泡：PC 已在上一周期改好，本周期不执行 ROM 输出。
                 * ROM 会在 27 MHz 时钟域内提前准备好新地址的指令。
                 */
                extra_cycle <= 1'b0;
            end else begin
                /* 普通指令默认顺序执行。 */
                pc <= pc + 1'b1;

                if (program_instruction == 12'h000) begin
                    /* NOP */
                end else if ((program_instruction & 12'hff8) == 12'h000 &&
                             program_instruction[2:0] == 3'd6) begin
                    /* TRIS GPIO：GP3 强制保持输入。 */
                    tris_gpio <= w[3:0] | 4'b1000;
                end else if ((program_instruction & 12'hfe0) == 12'h020) begin
                    /* MOVWF f */
                    write_file(file_address, w);
                end else if ((program_instruction & 12'hfe0) == 12'h060) begin
                    /* CLRF f */
                    write_file(file_address, 8'h00);
                    status[STATUS_Z] <= 1'b1;
                end else if ((program_instruction & 12'hfc0) == 12'h2c0) begin
                    /* DECFSZ f,d：结果为零时跳过下一条。 */
                    decremented_value = file_value - 1'b1;
                    if (destination_file)
                        write_file(file_address, decremented_value);
                    else
                        w <= decremented_value;

                    if (decremented_value == 0) begin
                        pc <= pc + 2'd2;
                        extra_cycle <= 1'b1;
                    end
                end else if ((program_instruction & 12'he00) == 12'ha00) begin
                    /* GOTO k：PIC10F200 的程序地址为 8 位。 */
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
