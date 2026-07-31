`timescale 1ns/1ps

/*
 * PIC10F200 真实固件端到端测试平台。
 *
 * 测试使用与 FPGA 位流构建相同的 firmware.mem，而不是在 testbench 中手写
 * 指令，因此能够发现 HEX 转换、同步程序存储器、CPU 取指和 GPIO 输出之间的
 * 集成问题。这里把每个 PIC 指令周期压缩为 4 个仿真时钟；程序存储器仍然是
 * 同步读取，并在两次 cpu_ce 之间留出足够时间准备下一条指令。27 MHz 到
 * 1 MHz 的真实分频不属于这个快速固件测试，避免为了观察 2 秒闪烁而仿真约
 * 5940 万个板级时钟周期；综合和板级验证仍需单独执行。
 *
 * 当前自动判定面向 blink 固件：GP0 必须配置为输出，并以约 100 万个 PIC
 * 指令周期为间隔交替翻转。VCD 同时保留全部关键信号，便于使用 GTKWave
 * 检查 GPIO 输出和方向的具体时序。PC、W 和 STATUS 每个指令周期都会变化，
 * 默认写入会让两秒波形膨胀到数百 MB，因此由控制台在引脚翻转时输出 PC。
 */
module pic10f200_firmware_tb;
    /*
     * 仿真脚本会在专用 build 目录中启动 vvp，所以这里使用稳定的相对文件名。
     * 这样避免 Windows 绝对路径中的反斜杠经过 PowerShell 和 Icarus 两层解析。
     */
    parameter FIRMWARE_MEM = "firmware_sim.mem";
    parameter VCD_FILE = "firmware_sim.vcd";

    reg clk = 0;
    reg reset_n = 0;
    reg [1:0] ce_divider = 0;
    wire cpu_ce = (ce_divider == 2'd3);

    wire [7:0] program_address;
    wire [11:0] program_instruction;
    wire [3:0] gpio_output;
    wire [3:0] gpio_direction;
    wire [7:0] debug_w;
    wire [7:0] debug_status;
    wire [7:0] debug_pc;

    integer instruction_cycles = 0;
    integer transition_count = 0;
    integer previous_transition_cycle = 0;
    integer interval;
    reg previous_gp0 = 0;

    /*
     * 4 个仿真时钟构成一个 1 us PIC 指令周期，所以 VCD 横轴仍表示真实的
     * PIC 虚拟时间：波形中相邻两次 blink 翻转约相隔 1 秒。
     */
    always #125 clk = ~clk;

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n)
            ce_divider <= 0;
        else
            ce_divider <= ce_divider + 1'b1;
    end

    pic10f200_program_memory #(
        .INIT_FILE(FIRMWARE_MEM)
    ) program_memory (
        .clk(clk),
        .address(program_address),
        .instruction(program_instruction)
    );

    pic10f200_core cpu (
        .clk(clk),
        .reset_n(reset_n),
        .cpu_ce(cpu_ce),
        .program_address(program_address),
        .program_instruction(program_instruction),
        .gpio_input(4'b1000),
        .gpio_output(gpio_output),
        .gpio_direction(gpio_direction),
        .debug_w(debug_w),
        .debug_status(debug_status),
        .debug_pc(debug_pc)
    );

    always @(posedge clk) begin
        if (reset_n && cpu_ce) begin
            instruction_cycles = instruction_cycles + 1;

            if (gpio_output[0] != previous_gp0) begin
                transition_count = transition_count + 1;
                $display(
                    "INFO: GP0=%0d at PIC instruction cycle %0d (PC=%02x)",
                    gpio_output[0], instruction_cycles, debug_pc
                );

                /*
                 * 第一次翻转包含固件初始化时间；从第二次开始才是完整的
                 * __delay_ms(1000) 保持区间。允许 1% 误差，以兼容 XC8 在
                 * 延时循环前后插入的少量指令。
                 */
                if (transition_count >= 2) begin
                    interval = instruction_cycles - previous_transition_cycle;
                    if (interval < 990000 || interval > 1010000) begin
                        $display(
                            "FAIL: GP0 interval=%0d cycles, expected about 1000000",
                            interval
                        );
                        $finish_and_return(1);
                    end
                end

                previous_transition_cycle = instruction_cycles;
                previous_gp0 = gpio_output[0];
            end

            if (transition_count >= 3) begin
                if (gpio_direction[0] !== 1'b0) begin
                    $display("FAIL: GP0 is not configured as output");
                    $finish_and_return(1);
                end
                $display(
                    "PASS: real firmware toggles GP0 at approximately 1 second intervals"
                );
                $finish;
            end

            if (instruction_cycles >= 2200000) begin
                $display(
                    "FAIL: firmware did not produce three GP0 transitions in 2200000 cycles"
                );
                $finish_and_return(1);
            end
        end
    end

    initial begin
        /*
         * 自动回归可传入 +NO_WAVE 以跳过体积较大的波形文件；默认保留 VCD，
         * 方便用户直接用 GTKWave 检查真实固件的引脚活动。
         */
        if (!$test$plusargs("NO_WAVE")) begin
            $dumpfile(VCD_FILE);
            /*
             * 只记录用户判断引脚功能所需的信号。不要对整个 DUT 执行
             * $dumpvars，否则延时循环中的 PC 变化会产生数百 MB 数据。
             */
            $dumpvars(0, gpio_output);
            $dumpvars(0, gpio_direction);
        end

        /* 先保持多个时钟周期，使同步 ROM 在 CPU 首次执行前读出地址 0。 */
        #1000 reset_n = 1;
    end
endmodule
