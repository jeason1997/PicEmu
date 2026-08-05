# PicEmu

PicEmu 是一个面向学习、验证和嵌入式移植的 PIC10 系列模拟器项目。核心使用
C11 编写，可直接加载 MPLAB XC8 生成的 Intel HEX 固件；同一套 PIC CPU
逻辑可以运行在命令行、Web 电路实验台、STM32F103 和 PY32F002A 上。
项目还包含一个独立的 Verilog PIC10F200 核心，可在 Tang Nano 1K FPGA
上执行真实 PIC 固件。

目前软件模拟器支持 PIC10F200 和 PIC10F202。PIC10F204/206 的模拟比较器
尚未实现。

## 能做什么

| 模块 | 主要用途 |
|---|---|
| 命令行模拟器 | 执行 HEX、跟踪指令、反汇编、断点、寄存器/RAM 转储、GPIO 事件和 VCD 波形 |
| Web 电路实验台 | 拖放器件、连线、多选编辑、加载/保存电路、运行/暂停/单步、查看寄存器、RAM、程序和 SPI Flash |
| STM32F103 端口 | 把虚拟 PIC GPIO 映射到真实 STM32 引脚，在裸机环境实时运行 PIC 固件 |
| PY32F002A 端口 | 在低成本 Cortex-M0+ 芯片上解释执行 PIC 固件，并把虚拟 GPIO 映射到实物引脚 |
| Tang Nano 1K FPGA | 用 Verilog 实现 PIC10F200 核心，固件随位流保存并从片上 BSRAM 取指 |

Web 使用 `examples/*/diagram.json` 描述电路。每颗 MCU 可以设置独立 HEX，
电路可包含多颗 PIC；器件模型、CPU 核心和前端界面彼此解耦，方便继续
增加芯片型号和外设。

## 快速开始

开发环境以 Linux/WSL 为主。基础命令行模拟器只需要 GCC 和 Make：

```sh
make
./build/picemu path/to/firmware.hex --cycles 2500000
```

安装XC8和DFP后，可以构建项目自带固件并运行全部自动测试：

```sh
make firmware
./build/picemu examples/blink/build/firmware.hex --cycles 2500000
make test
```

启动 Web 电路实验台：

```sh
# Linux/WSL
make run-web EXAMPLE=button

# Windows PowerShell
.\frontends\web\scripts\start.ps1 -Example button
```

裸机端口和 FPGA 的构建、烧录依赖各自工具链，参见：

- [STM32F103 移植与烧录](docs/ports/stm32f103.md)
- [PY32F002A 移植与烧录](docs/ports/py32f002a.md)
- [Tang Nano 1K FPGA 实现](fpga/README.md)

## 示例

| 示例 | 功能 |
|---|---|
| `blink` | GP0 一秒亮、一秒灭 |
| `breathing_led` | 软件 PWM 呼吸灯 |
| `button` | 按键控制两个 LED 和蜂鸣器 |
| `buzzer` | 按键触发蜂鸣器 |
| `led_chaser` | GP0～GP2 三路流水灯 |
| `playmusic` | 用软件方波播放一段简单旋律 |
| `spi_flash` | PIC10F202用通用软件SPI接口向W25Q写入并读回`HelloWorld` |
| `seven_segment` | 用三线74HC595时序驱动七段数码管循环显示0～9 |

每个示例拥有独立的 `main.c`、`Makefile`、`diagram.json` 和 `build/`。
详细构建方式和平台兼容性见[示例说明](docs/examples.md)。

## 项目结构

```text
PicEmu/
├── include/picemu/         公共 C 接口
├── src/                    CPU、HEX、电路网络、虚拟器件和命令行实现
├── frontends/
│   └── web/                Web 页面、本地服务和 C 调试后端
├── examples/               PIC XC8 固件及 Web 电路图
├── ports/stm32f103/        STM32F103 裸机端口
├── ports/py32f002a/        PY32F002A 裸机端口及所需设备支持文件
├── fpga/                   Tang Nano 1K Verilog 实现
├── tests/                  单元、集成和 Web 后端测试
└── docs/                   分主题文档
```

更完整的依赖关系和扩展方式见[项目架构](docs/architecture.md)。

## 文档

从[文档导航](docs/README.md)可以按任务查找说明，常用入口如下：

- [PIC10F200/PIC10F202 引脚与型号差异](docs/pic10f200.md)
- [在 Linux 下使用 XC8 编译固件](docs/firmware.md)
- [命令行模拟器](docs/cli.md)
- [Web 电路仿真器](docs/web-simulator.md)
- [测试、已实现功能和限制](docs/testing.md)

## 当前边界

PicEmu 的目标是数字逻辑和固件行为仿真，不是 SPICE：

- 不模拟电源电压、温度、模拟波形或器件损坏；
- Web 当前还内置可配置容量和实时查看数据的 W25Q SPI Flash；
- 没有 ELF/DWARF 源码级调试；
- PIC10F204/206 比较器、更多 PIC 系列和复杂外设仍待扩展；
- FPGA 核心目前只实现运行已有验证示例所需的 PIC10F200 指令和资源。

具体实现程度以[测试与功能范围](docs/testing.md)和
[FPGA 当前实现范围](fpga/README.md#当前实现范围)为准。
