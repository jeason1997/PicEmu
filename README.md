# PIC10F200 教学模拟器

这是一个使用 C11 编写的 PIC10F200 模拟器，可以在 Linux 下通过 GCC
编译，加载 MPLAB XC8 生成的 Intel HEX 固件并执行。除了命令行调试器，
项目还提供可交互的 SDL2 虚拟实验板，以及供 STM32 等嵌入式平台使用的
GPIO 映射接口。

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
│   ├── pic_device.h       芯片型号、封装引脚和能力描述
│   ├── sim_board.h        导线网络和虚拟实验板
│   ├── sim_device.h       LED、按键、蜂鸣器等设备接口
│   ├── pic_platform.h     映射真实硬件GPIO的平台接口
│   ├── disassembler.h     12位指令反汇编接口
│   └── vcd_writer.h       GTKWave波形输出接口
├── src/
│   ├── hex_loader.c       Intel HEX 解析与校验
│   ├── pic10f200.c        CPU、RAM、栈、复位和外设
│   ├── pic_device.c       PIC10F200设备描述及型号查找
│   ├── sim_board.c        引脚连线、电平解析和仿真调度
│   ├── sim_device.c       可扩展外围设备公共接口
│   ├── sim_led.c          LED电气模型
│   ├── sim_button.c       按键电气模型
│   ├── sim_buzzer.c       蜂鸣器电气模型
│   ├── circuit_config.c   JSON电路配置解析器
│   ├── pic_platform.c     虚拟引脚到平台GPIO的桥接
│   ├── disassembler.c     指令反汇编
│   ├── vcd_writer.c       VCD波形生成
│   └── main.c             命令行、事件和执行循环
├── frontends/sdl/
│   ├── main.c             仅负责窗口、输入和仿真时钟
│   ├── sdl_circuit.c      从配置装配和绘制完整电路
│   ├── sdl_part_*.c       每一种器件的独立可视模块
│   └── sdl_text.c         无外部字体依赖的位图文字
├── circuits/blink.json    按键、双LED和蜂鸣器电路配置
├── ports/stm32f103/       STM32F103移植说明和接口约定
├── tools/hex2c.c          将HEX转换成可烧录的C常量
├── test/
│   ├── Makefile           测试固件编译、运行和验证
│   ├── blink.c            XC8按键、双LED和蜂鸣器示例
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

如需构建 SDL2 前端，先安装开发包：

```sh
sudo apt install libsdl2-dev
make sdl
```

生成 `build/picemu-sdl`。

## SDL2 可交互实验板

运行配置好的演示电路：

```sh
make run-sdl
```

也可以明确指定配置文件；命令行的第二个参数可以覆盖 JSON 中的固件：

```sh
./build/picemu-sdl circuits/blink.json
./build/picemu-sdl circuits/blink.json another.hex
```

### JSON电路配置

配置采用类似 Wokwi `diagram.json` 的数据结构。目前实现
`version`、`firmware`、`parts` 和 `connections`：

```json
{
  "version": 1,
  "firmware": "test/build/blink.hex",
  "parts": [
    {
      "id": "mcu",
      "type": "pic10f200",
      "left": 330,
      "top": 150
    },
    {
      "id": "led0",
      "type": "led",
      "left": 150,
      "top": 200,
      "attrs": { "color": "red" }
    }
  ],
  "connections": [
    ["mcu:GP0", "led0:A", "red", []]
  ]
}
```

每个器件必须有唯一的 `id` 和已注册的 `type`。`left`、`top` 是
960×600 逻辑画布中的坐标。连接端点写成 `器件ID:引脚名`。当前支持：

| type | 可连接引脚 | attrs |
|---|---|---|
| `pic10f200` | `GP0`、`GP1`、`GP2`、`GP3`；`VDD/VSS`仅显示 | 无 |
| `led` | `A`或`IN` | `color`: red/green/blue/yellow |
| `pushbutton` | `1`或`OUT` | `activeLow`: true/false |
| `buzzer` | `1`或`IN` | 无 |

`connections` 的第三项是导线颜色，第四项为以后兼容走线指令预留，
目前会读取并忽略。配置解析器也会跳过未知对象字段，方便后续演进。

默认演示电路的连线为：

| PIC引脚 | 虚拟设备 | 行为 |
|---|---|---|
| GP0 | 红色LED | 高电平点亮 |
| GP1 | 绿色LED | 高电平点亮 |
| GP2 | 蜂鸣器 | 高电平发出约2 kHz声音 |
| GP3 | 按键 | 松开为高电平，按下为低电平 |

操作方式：

- 拖动窗口边缘可等比例缩放实验板，最小尺寸为 480×300；
- 鼠标按住板上的按键，向 GP3 输入低电平；
- `Space` 暂停或继续；
- 暂停时按 `N` 执行一条指令；
- `R` 复位芯片；
- `Esc` 退出。

前端没有把电路或设备行为写进主程序。`SimBoard` 用网络连接芯片引脚和
`SimDevice`；各器件的电气模型、SDL外观和JSON注册逻辑彼此分开。以后
增加屏幕、数码管或继电器时，增加对应模型与 `sdl_part_*.c`，再注册一种
`type` 即可，不需要修改 PIC10F200 指令执行器或 SDL 主循环。

## 移植到 STM32F103

模拟核心、设备描述和平台桥接代码只依赖标准 C，不依赖 SDL，也不动态
分配内存。桌面界面与核心分离，因此可以把这些文件加入 STM32 工程：

```text
src/hex_loader.c
src/pic_device.c
src/pic10f200.c
src/pic_platform.c
```

在 STM32 端实现 `PicPlatformOps` 的设置方向、写引脚、读引脚和读取时间
四个回调，即可把 GP0～GP3 映射到真实 GPIO。完整的回调示例、调度建议和
注意事项见 `ports/stm32f103/README.md`。

嵌入式设备通常没有文件系统，可在 PC 上把 HEX 转成 C 数组后随固件烧录：

```sh
make tools
./build/hex2c test/build/blink.hex blink > blink_firmware.c
```

生成文件提供 `const HexImage blink_image`。STM32 启动时可以直接把它交给
`pic10f200_init()`，无需在板上解析文件。

## 为其他 PIC 型号预留的结构

目前只实现 PIC10F200，但型号参数没有散落在 SDL 界面或虚拟设备中。
`PicDeviceDescription` 集中描述程序空间、RAM、栈、GPIO 数量、物理脚号
和引脚能力；当前实例为 `PIC_DEVICE_PIC10F200`。

将来增加 PIC10F202、PIC10F204 或 PIC10F206 时，建议按以下顺序扩展：

1. 添加对应的设备描述；
2. 对共用的 Baseline CPU 指令复用现有执行器；
3. 按型号挂接更大的程序空间、比较器等差异外设；
4. 让前端按设备描述生成引脚，而不是在前端判断型号；
5. 为新型号加入独立 HEX 和单元测试。

这次没有声称支持这些型号：设备查找器目前只接受 PIC10F200。预留层的
目的，是让以后增加型号时不必重写 SDL 设备、连线系统或 STM32 GPIO
桥接层。

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
2. 使用XC8重新编译`blink.c`，注入GP3按键事件，检查两颗LED反转和
   50ms蜂鸣器脉冲。

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
注入GP3按下/松开事件，检查GP0/GP1反转和GP2蜂鸣器脉冲
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
- SDL2交互式实验板、可扩展设备和连线网络。
- 面向STM32等平台的真实GPIO桥接接口及HEX转C工具。
- 集中的芯片型号与引脚能力描述，为后续型号预留扩展点。
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
