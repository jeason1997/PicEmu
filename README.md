# PIC10F200 教学模拟器

这是一个使用 C11 编写的 PIC10F200 模拟器，可以在 Linux 下通过 GCC
编译，加载 MPLAB XC8 生成的 Intel HEX 固件并执行。

项目强调结构清晰和中文注释，适合作为 MCU 指令集模拟器的入门示例，
不以替代 MPLAB SIM 为目标。

## PIC10F200 6引脚SOT-23封装

本项目模拟的是PIC10F200。它最典型的是6引脚SOT-23封装，包含2个电源
引脚和4个GPIO引脚：

```text
             PIC10F200
         ┌─────────────┐
 GP0  1 ─┤             ├─ 6  GP3/MCLR/VPP
 VSS  2 ─┤             ├─ 5  VDD
 GP1  3 ─┤             ├─ 4  GP2/T0CKI/FOSC4
         └─────────────┘
```

| 脚号 | 名称 | 方向 | 功能 |
|---:|---|---|---|
| 1 | GP0/ICSPDAT | 输入/输出 | 通用GPIO；在线串行编程数据线 |
| 2 | VSS | 电源 | 电源地/GND |
| 3 | GP1/ICSPCLK | 输入/输出 | 通用GPIO；在线串行编程时钟线 |
| 4 | GP2/T0CKI/FOSC4 | 输入/输出 | 通用GPIO；Timer0外部时钟输入；系统时钟四分频输出 |
| 5 | VDD | 电源 | 芯片正电源 |
| 6 | GP3/MCLR/VPP | 仅输入 | 通用数字输入；低有效复位输入；编程电压输入 |

### GP0/ICSPDAT

GP0可以作为普通数字输入或输出。烧录芯片时，它复用为ICSP编程数据线，
通常连接编程器的`PGD/ICSPDAT`。

```c
TRISGPIO = 0b00001110; /* GP0输出，其余GPIO输入 */
GP0 = 1;
```

### GP1/ICSPCLK

GP1可以作为普通数字输入或输出。烧录芯片时，它复用为ICSP编程时钟线，
通常连接编程器的`PGC/ICSPCLK`。

```c
TRISGPIO = 0b00001101; /* GP1输出，其余GPIO输入 */
GP1 = 1;
```

### GP2/T0CKI/FOSC4

GP2有三种主要用途：

- 普通数字输入/输出；
- 作为`T0CKI`向Timer0输入外部计数脉冲；
- 输出内部系统时钟的四分频信号`FOSC/4`。

当GP2被配置为T0CKI或FOSC4时，不能再同时作为普通GPIO输出使用。

### GP3/MCLR/VPP

GP3是特殊的仅输入引脚，不能输出。它还可以复用为：

- `MCLR`：外部低有效复位输入；
- `VPP`：进入ICSP编程模式所需的编程控制/电压输入。

即使把`TRISGPIO`的bit 3写成0，GP3仍然保持输入状态。

### TRISGPIO方向设置

`TRISGPIO`只有低4位与PIC10F200的GPIO有关：

```text
bit 3  bit 2  bit 1  bit 0
 GP3    GP2    GP1    GP0
```

方向规则：

```text
TRIS位=1：输入
TRIS位=0：输出
```

GP3是例外，它始终只能作为输入。bit 4～bit 7没有对应的GPIO，写入后
没有实际作用。

| TRISGPIO低4位 | GP3 | GP2 | GP1 | GP0 |
|---|---|---|---|---|
| `1111` | 输入 | 输入 | 输入 | 输入 |
| `1110` | 输入 | 输入 | 输入 | 输出 |
| `1100` | 输入 | 输入 | 输出 | 输出 |
| `1000` | 输入 | 输出 | 输出 | 输出 |
| `0000` | 仍为输入 | 输出 | 输出 | 输出 |

PIC10F200没有GP4、GP5或GP6，也没有ADC、PWM、UART、SPI和I²C。PIC10F204
和PIC10F206虽然封装相似，但额外带模拟比较器，不能把这些功能用于
PIC10F200。

## 项目结构

```text
PicEmu/
├── include/
│   ├── hex_loader.h       Intel HEX 加载器接口
│   ├── pic10f200.h        PIC10F200 模拟器接口和状态定义
│   ├── disassembler.h     12位指令反汇编接口
│   └── vcd_writer.h       GTKWave波形输出接口
├── src/
│   ├── hex_loader.c       Intel HEX 解析与校验
│   ├── pic10f200.c        CPU、RAM、栈、复位和外设
│   ├── disassembler.c     指令反汇编
│   ├── vcd_writer.c       VCD波形生成
│   └── main.c             命令行、事件和执行循环
├── test/
│   ├── Makefile           测试固件编译、运行和验证
│   ├── blink.c            XC8 闪灯示例
│   ├── pin_events.example 外部引脚事件示例
│   ├── test_blink.sh      端到端自动测试
│   └── unit/test_cpu.c    CPU和外设单元测试
├── build/                 GCC 构建产物，由 make 自动创建
│   ├── obj/               .o 和 .d 中间文件
│   └── picemu             最终模拟器程序
├── Makefile
└── README.md
```

`src/` 只保存源代码。GCC 产生的目标文件、依赖文件和最终程序全部进入
根目录的 `build/`；XC8 产生的测试固件和中间文件全部进入
`test/build/`。两个目录都可以通过 `make clean` 安全删除和重建。

## 构建模拟器

需要 GCC、Make 和一个支持 C11 的 Linux 环境：

```sh
make
```

生成结果：

```text
build/picemu
build/obj/main.o
build/obj/hex_loader.o
build/obj/pic10f200.o
```

运行已有固件：

```sh
./build/picemu test/build/blink.hex --cycles 3500000
```

完整命令行参数：

```text
--cycles N          最多模拟N个PIC指令周期
--quiet             不打印GPIO变化
--trace             逐条显示PC、机器码、反汇编和寄存器状态
--disassemble       反汇编HEX程序并退出
--break ADDRESS     执行到指定PC前停止
--dump              结束时显示寄存器、硬件栈和RAM
--vcd FILE          生成GTKWave可读取的波形文件
--events FILE       按周期向GPIO注入外部事件
```

反汇编固件：

```sh
./build/picemu test/build/blink.hex --disassemble
```

跟踪前100个周期并显示最终状态：

```sh
./build/picemu test/build/blink.hex \
  --cycles 100 --trace --dump
```

在程序地址 `0x00E` 设置断点：

```sh
./build/picemu test/build/blink.hex --break 0x00E --dump
```

生成波形并使用GTKWave查看：

```sh
./build/picemu test/build/blink.hex \
  --cycles 10000 --vcd build/blink.vcd
gtkwave build/blink.vcd
```

VCD包含PC、W、STATUS、TMR0、GPIO总线、`gp0`～`gp3`四条独立引脚
信号和Sleep状态。这样即使GTKWave版本不支持展开总线，也能直接观察
每个GPIO。长时间运行会产生较大的波形文件，建议配合较小的
`--cycles` 使用。

## 外部引脚事件

`--events` 可以模拟按键或送入T0CKI的方波。事件文件格式：

```text
# cycle pin value
1000 GP3 0
2000 GP3 z
3000 GP2 0
3100 GP2 1
```

电平可为 `0`、`1` 或 `z`，其中 `z` 表示外部电路释放引脚。示例：

```sh
./build/picemu test/build/blink.hex \
  --cycles 5000 --events test/pin_events.example
```

事件会在当前PIC指令边界注入。如果目标周期落在一条两周期指令内部，
事件会在该指令结束后的第一个边界生效。

## 在 Linux 命令行编译 PIC 固件

测试固件使用 Microchip MPLAB XC8，不需要启动 MPLAB X IDE。PIC10F200
还需要 `PIC10-12Fxxx_DFP` Device Family Pack。

XC8 4.00 的常见安装位置是：

```text
/opt/microchip/xc8/v4.00/bin/xc8-cc
```

建议将 DFP 解压到：

```text
~/.mchp_packs/Microchip/PIC10-12Fxxx_DFP/<版本>/xc8
```

`test/Makefile` 会自动在上述位置寻找已安装版本。也可以显式指定：

```sh
export PIC10_DFP="$HOME/.mchp_packs/Microchip/PIC10-12Fxxx_DFP/1.9.189/xc8"
```

从项目根目录编译固件：

```sh
make firmware
```

也可以进入测试目录独立编译：

```sh
make -C test firmware
```

如果 DFP 安装在其他位置：

```sh
make -C test firmware \
  DFP=/path/to/PIC10-12Fxxx_DFP/version/xc8
```

XC8 生成的所有文件都位于：

```text
test/build/
```

其中模拟器使用的是 `test/build/blink.hex`；`blink.elf`、汇编文件、符号表
和其他 XC8 中间文件也会保留在同一目录，便于学习和分析。

## 运行和测试

编译模拟器与固件，然后运行固件：

```sh
make run
```

执行完整的端到端测试：

```sh
make test
```

`make test`现在包含两层测试：

1. 不依赖XC8的CPU单元测试，覆盖算术标志、跳过指令、CALL/RETLW、
   间接寻址、Timer0写入抑制、WDT/Sleep、T0CKI和反汇编。
2. 使用XC8重新编译`blink.c`，加载真实HEX并检查GPIO翻转和周期。

也可以只执行快速单元测试：

```sh
make unit-test
```

测试流程为：

```text
test/blink.c
    -> XC8
test/build/blink.hex
    -> build/picemu
检查 GP0 是否按 1,0,1,0 翻转，并比较高低电平持续周期
```

也可以在 `test/` 中分别执行：

```sh
make -C test run
make -C test test
make -C test help
```

## 清理

在项目根目录执行：

```sh
make clean
```

它会删除：

```text
build/
test/build/
```

不会删除 `src/`、`include/`、`test/blink.c` 或测试脚本。

## 已实现范围

- PIC10F200 的 12 位 Baseline 指令译码。
- W、PC、STATUS、FSR、PCL 和两级硬件栈。
- 16 字节通用 RAM 与 INDF/FSR 间接寻址。
- CALL、RETLW、GOTO、计算跳转和跳过指令周期。
- GPIO/TRIS，包括 GP3 仅输入及经典 PIC 读-修改-写行为。
- GPIO弱上拉、外部引脚驱动和引脚变化Sleep唤醒。
- Timer0内部时钟、外部GP2/T0CKI、边沿选择、预分频和写入抑制。
- 由配置字WDTE控制的WDT、预分频、运行时复位及Sleep唤醒。
- POR、MCLR和WDT复位入口及TO/PD状态。
- Intel HEX 数据、EOF、扩展段地址及扩展线性地址记录。
- MPLAB/XC8 放在字节地址 `0x1FFE` 的配置字。
- 反汇编、逐指令跟踪、地址断点、状态/RAM转储。
- 外部引脚事件文件和VCD波形输出。
- CPU单元测试和XC8固件端到端测试。

## 当前限制

当前仍未完整模拟：

- WDT振荡器随电压、温度和芯片个体变化产生的误差
- MCLR引脚的异步电平事件；核心已经提供MCLR复位类型
- 引脚输出冲突和模拟电压
- 交互式单步调试器、条件断点和数据观察点
- 振荡器误差与电气特性
- ELF/DWARF 源代码级调试

模拟器使用“PIC 指令周期”作为时间单位。传统 PIC 的指令周期频率通常为
`Fosc / 4`；若内部振荡器为 4 MHz，一个指令周期约为 1 微秒。
