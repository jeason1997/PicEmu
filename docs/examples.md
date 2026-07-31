# 示例说明

`examples/` 中每个目录都是一个独立 PIC 固件工程：

```text
examples/<名称>/
├── main.c          XC8 源码
├── Makefile        调用 examples/common.mk
├── diagram.json    SDL 和 Web 共用电路图
├── events.txt      可选，命令行 GPIO 输入事件
└── build/          自动生成的 ELF/HEX 等文件
```

## 构建和运行

构建全部示例：

```sh
make firmware
```

构建单个示例：

```sh
make -C examples/blink firmware
```

在不同前端运行：

```sh
make run EXAMPLE=blink
make run-sdl EXAMPLE=blink
make run-web EXAMPLE=blink
```

Windows PowerShell 可以直接启动 Web、STM32 或 FPGA 流程：

```powershell
.\frontends\web\scripts\start.ps1 -Example blink
.\ports\stm32f103\scripts\program.ps1 -Example blink
.\fpga\scripts\program.ps1 -Example blink
```

后两条命令是否成功还取决于目标硬件、工具链和当前 FPGA 指令实现范围。

## 示例列表

| 示例 | PIC 固件行为 | 电路外设 | 主要用途 |
|---|---|---|---|
| `blink` | GP0 每秒翻转 | 红色 LED | 最小构建、计时和 GPIO 输出测试 |
| `breathing_led` | GP0 软件 PWM | 红色 LED | PWM 占空比和亮度显示 |
| `button` | GP3 按键输入，控制 GP0/GP1/GP2 | 两个 LED、按键、蜂鸣器 | GPIO 输入输出组合 |
| `buzzer` | GP3 触发 GP2 脉冲 | 按键、蜂鸣器 | 外部输入和声音 |
| `led_chaser` | GP0～GP2 依次点亮 | 红绿蓝三个 LED | 多路 GPIO 输出 |
| `playmusic` | GP2 产生不同频率方波 | 无源蜂鸣器 | 软件音调和完整程序空间测试 |
| `spi_flash` | PIC10F202通过GP0～GP3软件模拟SPI，写入并读回 `HelloWorld` | W25Q、绿色结果 LED | 通用缓冲区读写接口和SPI Mode 0 |
| `seven_segment` | GP0发送数据、GP1提供移位时钟、GP2产生锁存脉冲 | 74HC595驱动的单位七段数码管 | 三线串行移位、段码与锁存时序 |

## 平台说明

- 命令行、SDL 和 Web 使用同一个 C PIC 核心，所有示例都应通过集成测试。
- STM32F103 在构建时把所选 HEX 转换为 C 数组，因此运行时不需要文件系统。
- FPGA 会拒绝超出当前 64 字程序窗口或使用未实现指令的固件；当前已实板验证
  `blink`、`button` 和 `led_chaser`。详细范围见
  [FPGA 文档](../fpga/README.md#当前实现范围)。
- `diagram.json` 属于示例的一部分。在 Web 中编辑项目示例后点击“保存电路”，
  会直接更新对应文件。
