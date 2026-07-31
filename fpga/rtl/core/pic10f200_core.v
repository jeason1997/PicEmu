/*
 * PIC10F200 Baseline 12 位 CPU 核心。
 *
 * 采用共享 ALU 和单一文件寄存器写回端口，避免小容量 FPGA 因每条指令各自
 * 展开写入多路器而耗尽 LUT。实现 33 条指令、16 字节 GPR、间接寻址、两级
 * 循环栈、Timer0、WDT、Sleep 和 GPIO。cpu_ce 表示一个 1 us 指令周期。
 */
module pic10f200_core #(
    parameter WATCHDOG_ENABLED=1'b0,
    parameter integer WDT_BASE_CYCLES=18000
) (
    input wire clk, reset_n, cpu_ce,
    output wire [7:0] program_address,
    input wire [11:0] program_instruction,
    input wire [3:0] gpio_input,
    output wire [3:0] gpio_output, gpio_direction,
    output wire [7:0] debug_w, debug_status, debug_pc
);
    localparam C=0, DC=1, Z=2, PD=3, TO=4;
    reg [7:0] w, pc, status, fsr, osccal, tmr0, option_reg;
    reg [3:0] gpio_latch, tris_gpio, previous_gpio;
    reg [7:0] stack0, stack1;
    reg stack_pointer, bubble, sleeping, previous_t0cki;
    reg [7:0] prescaler;
    reg [1:0] timer0_inhibit;
    reg [31:0] watchdog;

    wire [4:0] f=program_instruction[4:0];
    wire [4:0] write_address=(f==0)?fsr[4:0]:f;
    wire [4:0] read_address=(f==0)?fsr[4:0]:f;
    wire d=program_instruction[5];
    wire [2:0] bit_index=program_instruction[7:5];
    wire [3:0] direction=tris_gpio|4'b1000;
    wire [3:0] pins=(gpio_latch&~direction)|(gpio_input&direction);
    reg [7:0] file_value;

    /* 统一 ALU/写回控制。 */
    reg file_we, w_we, z_we, c_we, dc_we, skip;
    reg [7:0] write_value;
    reg next_c, next_dc;
    reg normal_alu;
    wire [7:0] literal=program_instruction[7:0];
    wire [3:0] operation=program_instruction[9:6];
    wire [8:0] add_value={1'b0,file_value}+{1'b0,w};
    wire gpr_write_enable=cpu_ce && !sleeping && !bubble && file_we &&
                          write_address>=16;
    wire [7:0] gpr_data;

    /*
     * GW1NZ-1 的 16×4 双口分布式 SRAM 正好组成 16×8 GPR。显式实例化可避免
     * 综合器把异步读 RAM 展开为 128 个触发器和多级宽 MUX。
     */
    \$__GOWIN_LUTRAM_ #(.BITS_USED(4'b1111)) gpr_low (
        .PORT_R_RD_DATA(gpr_data[3:0]), .PORT_W_WR_DATA(write_value[3:0]),
        .PORT_W_ADDR(write_address[3:0]), .PORT_R_ADDR(read_address[3:0]),
        .PORT_W_WR_EN(gpr_write_enable), .PORT_W_CLK(clk)
    );
    \$__GOWIN_LUTRAM_ #(.BITS_USED(4'b1111)) gpr_high (
        .PORT_R_RD_DATA(gpr_data[7:4]), .PORT_W_WR_DATA(write_value[7:4]),
        .PORT_W_ADDR(write_address[3:0]), .PORT_R_ADDR(read_address[3:0]),
        .PORT_W_WR_EN(gpr_write_enable), .PORT_W_CLK(clk)
    );

    assign program_address=pc;
    assign gpio_output=gpio_latch;
    assign gpio_direction=direction;
    assign debug_w=w;
    assign debug_status=status;
    assign debug_pc=pc;

    always @* begin
        case(read_address)
            0: file_value=0;
            1: file_value=tmr0;
            2: file_value=pc;
            3: file_value=status;
            4: file_value=fsr;
            5: file_value=osccal;
            6: file_value={4'h0,pins};
            default: file_value=(read_address>=16)?gpr_data:0;
        endcase
    end

    /*
     * 字节指令共享一个 ALU。operation 对应 opcode<9:6>，仅在 0x080~0x3ff
     * 范围启用；跳过类不更新 Z，旋转类只更新 C。
     */
    always @* begin
        file_we=0; w_we=0; z_we=0; c_we=0; dc_we=0; skip=0;
        write_value=0; next_c=status[C]; next_dc=status[DC]; normal_alu=0;
        if((program_instruction&12'hfc0)>=12'h080 &&
           (program_instruction&12'hfc0)<=12'h3c0) begin
            normal_alu=1;
            case(operation)
                4'h2: begin write_value=file_value-w; c_we=1; dc_we=1;
                    next_c=(file_value>=w); next_dc=(file_value[3:0]>=w[3:0]); z_we=1; end
                4'h3: begin write_value=file_value-1'b1; z_we=1; end
                4'h4: begin write_value=file_value|w; z_we=1; end
                4'h5: begin write_value=file_value&w; z_we=1; end
                4'h6: begin write_value=file_value^w; z_we=1; end
                4'h7: begin write_value=add_value[7:0]; c_we=1; dc_we=1;
                    next_c=add_value[8];
                    next_dc=({1'b0,file_value[3:0]}+{1'b0,w[3:0]})>15; z_we=1; end
                4'h8: begin write_value=file_value; z_we=1; end
                4'h9: begin write_value=~file_value; z_we=1; end
                4'ha: begin write_value=file_value+1'b1; z_we=1; end
                4'hb: begin write_value=file_value-1'b1; skip=(write_value==0); end
                4'hc: begin write_value={status[C],file_value[7:1]}; c_we=1; next_c=file_value[0]; end
                4'hd: begin write_value={file_value[6:0],status[C]}; c_we=1; next_c=file_value[7]; end
                4'he: write_value={file_value[3:0],file_value[7:4]};
                4'hf: begin write_value=file_value+1'b1; skip=(write_value==0); end
                default: normal_alu=0;
            endcase
            if(normal_alu) begin file_we=d; w_we=!d; end
        end
        if((program_instruction&12'hf00)==12'h600) skip=!file_value[bit_index];
        if((program_instruction&12'hf00)==12'h700) skip=file_value[bit_index];
        if((program_instruction&12'hfe0)==12'h020) begin
            file_we=1; w_we=0; write_value=w;                         /* MOVWF */
        end
        if((program_instruction&12'hfe0)==12'h060) begin
            file_we=1; w_we=0; write_value=0; z_we=1;                 /* CLRF */
        end
        if((program_instruction&12'hf00)==12'h400) begin
            file_we=1; w_we=0; write_value=file_value&~(8'h01<<bit_index);
        end
        if((program_instruction&12'hf00)==12'h500) begin
            file_we=1; w_we=0; write_value=file_value|(8'h01<<bit_index);
        end
    end

    task timer_tick;
        begin
            if(!option_reg[5]) begin
                if(timer0_inhibit!=0) timer0_inhibit<=timer0_inhibit-1'b1;
                else if(option_reg[3]) tmr0<=tmr0+1'b1;
                else if(prescaler==((1<<(option_reg[2:0]+1))-1)) begin
                    prescaler<=0; tmr0<=tmr0+1'b1;
                end else prescaler<=prescaler+1'b1;
            end
        end
    endtask

    always @(posedge clk or negedge reset_n) begin
        if(!reset_n) begin
            w<=0; pc<=0; status<=8'h18; fsr<=0; osccal<=0; tmr0<=0;
            option_reg<=8'hff; gpio_latch<=0; tris_gpio<=4'hf;
            stack0<=0; stack1<=0; stack_pointer<=0; bubble<=0; sleeping<=0;
            previous_t0cki<=0; previous_gpio<=0; prescaler<=0;
            timer0_inhibit<=0; watchdog<=0;
        end else if(cpu_ce) begin
            previous_t0cki<=pins[2]; previous_gpio<=pins;

            /* 外部 Timer0：T0SE=0 上升沿，T0SE=1 下降沿。 */
            if(option_reg[5] && previous_t0cki!=pins[2] &&
               pins[2]!=option_reg[4]) begin
                if(option_reg[3]) tmr0<=tmr0+1'b1;
                else if(prescaler==((1<<(option_reg[2:0]+1))-1)) begin
                    prescaler<=0; tmr0<=tmr0+1'b1;
                end else prescaler<=prescaler+1'b1;
            end
            if(sleeping && !option_reg[7] &&
               |((previous_gpio^pins)&4'b1011)) sleeping<=0;

            if(WATCHDOG_ENABLED) begin
                if(watchdog+1 >=
                   (WDT_BASE_CYCLES << (option_reg[3]?option_reg[2:0]:0))) begin
                    watchdog<=0;
                    if(sleeping) begin sleeping<=0; status[TO]<=0; status[PD]<=0; end
                    else begin
                        pc<=0; w<=0; status<=8'h08; fsr<=0; osccal<=0; tmr0<=0;
                        option_reg<=8'hff; gpio_latch<=0; tris_gpio<=4'hf;
                        stack0<=0; stack1<=0; stack_pointer<=0; bubble<=0;
                        prescaler<=0; timer0_inhibit<=0;
                    end
                end else watchdog<=watchdog+1'b1;
            end

            if(!sleeping) begin
                timer_tick();
                if(bubble) bubble<=0;
                else begin
                    pc<=pc+1'b1;
                    /* 全部文件类指令共用唯一写回端口，INDF 已解析为 write_address。 */
                    if(file_we && write_address!=0) case(write_address)
                        1: begin tmr0<=write_value; prescaler<=0; timer0_inhibit<=3; end
                        2: pc<=write_value;
                        3: status<={3'b000,status[4:3],write_value[2:0]};
                        4: fsr<=write_value;
                        5: osccal<=write_value;
                        6: gpio_latch<=write_value[3:0];
                        default: begin end
                    endcase
                    if(w_we) w<=write_value;
                    if(z_we) status[Z]<=(write_value==0);
                    if(c_we) status[C]<=next_c;
                    if(dc_we) status[DC]<=next_dc;

                    if(program_instruction==12'h002) begin
                        option_reg<=w; prescaler<=0; watchdog<=0;             /* OPTION */
                    end else if(program_instruction==12'h003) begin
                        sleeping<=1; watchdog<=0; status[PD]<=0; status[TO]<=1;
                    end else if(program_instruction==12'h004) begin
                        watchdog<=0; prescaler<=0; status[PD]<=1; status[TO]<=1;
                    end else if((program_instruction&12'hff8)==0 &&
                                program_instruction[2:0]>=5) begin
                        if(program_instruction[2:0]==6) tris_gpio<=w[3:0]|4'b1000;
                    end else if((program_instruction&12'hfe0)==12'h040) begin w<=0; status[Z]<=1; end
                    else if((program_instruction&12'hf00)==12'h800) begin
                        w<=literal; stack_pointer<=stack_pointer-1'b1;
                        pc<=stack_pointer?stack0:stack1; bubble<=1;           /* RETLW */
                    end else if((program_instruction&12'hf00)==12'h900) begin
                        if(stack_pointer) stack1<=pc+1'b1; else stack0<=pc+1'b1;
                        stack_pointer<=stack_pointer+1'b1; pc<=literal; bubble<=1;
                    end else if((program_instruction&12'he00)==12'ha00) begin
                        pc<=literal; bubble<=1;
                    end else if((program_instruction&12'hf00)==12'hc00) w<=literal;
                    else if((program_instruction&12'hf00)==12'hd00) begin w<=w|literal; status[Z]<=((w|literal)==0); end
                    else if((program_instruction&12'hf00)==12'he00) begin w<=w&literal; status[Z]<=((w&literal)==0); end
                    else if((program_instruction&12'hf00)==12'hf00) begin w<=w^literal; status[Z]<=((w^literal)==0); end

                    if(skip) begin pc<=pc+2'd2; bubble<=1; end
                end
            end
        end
    end
endmodule
