# Tang Nano 1K 上的 PIC10F200

本目录是在 Tang Nano 1K（GW1NZ-LV1QN48C6/I5）中实现 PIC10F200 的
FPGA 版本。它会加载 XC8 编译生成的 Intel HEX，转换为 FPGA 程序 ROM，
然后由 Verilog CPU 核心取指和执行，不是把示例行为写死在 RTL 中。

## 当前阶段

第一阶段已经可以在实板运行 `examples/blink/build/firmware.hex`：

- 12 位指令取指和译码；
- 8 位 W、PC 和 STATUS；
- GPIO 输出锁存器和 TRIS 方向控制；
- GP3 固定输入；
- XC8 一秒 blink 使用的三个 GPR（`0x10`～`0x12`）；
- NOP、TRIS、MOVWF、CLRF、DECFSZ、GOTO、MOVLW；
- 跳转和成功跳过指令的第二个指令周期；
- 27 MHz FPGA 时钟产生精确的 1 MHz PIC 指令周期。

这还是逐步实现中的核心。目前程序 ROM 窗口为 64 字，尚未实现全部 16 字节
RAM、完整指令集、两级栈、Timer0、WDT、SLEEP 和外部 T0CKI。HEX 转换器会
拒绝超出当前容量或包含未实现指令的固件，避免静默执行错误。

## 引脚映射

| PIC10F200 | Tang Nano 1K | 功能 |
|---|---:|---|
| GP0 | pin 9 | 红色 LED |
| GP1 | pin 10 | 蓝色 LED |
| GP2 | pin 11 | 绿色 LED |
| GP3 | pin 13 / BTN1 | PIC 输入 |
| 复位 | pin 44 / BTN2 | FPGA/PIC 核心复位 |
| 时钟 | pin 47 | 板载 27 MHz |

板载 RGB LED 低电平点亮。顶层已经处理该电气极性，PIC 固件仍按普通逻辑
编写：向 GPx 写 `1` 表示对应虚拟引脚为高电平。

## 目录结构

```text
fpga/
├── boards/tang_nano_1k/       # 板级顶层和 CST 引脚约束
├── rtl/core/                  # PIC10F200 CPU 与程序 ROM
├── rtl/common/                # 时钟使能、输入同步等可复用模块
├── tb/                        # RTL 测试平台
├── tools/                     # Intel HEX 转 ROM 工具
├── scripts/                   # Windows 构建、仿真、烧录脚本
├── pic10f200.lushay.json      # Lushay Code 工程入口
└── build/                     # 自动生成，不提交 Git
```

## Windows 命令

脚本默认使用 `E:\oss-cad-suite`。安装位置不同可以设置：

```powershell
$env:OSS_CAD_SUITE = "D:\tools\oss-cad-suite"
```

运行核心测试：

```powershell
.\fpga\scripts\simulate.ps1
```

只构建默认 blink 固件：

```powershell
.\fpga\scripts\build.ps1
```

选择其他 HEX：

```powershell
.\fpga\scripts\build.ps1 -Firmware "path\to\firmware.hex"
```

构建并烧录到 Tang Nano 1K SRAM：

```powershell
.\fpga\scripts\program.ps1
```

选择固件后构建并烧录：

```powershell
.\fpga\scripts\program.ps1 -Firmware "path\to\firmware.hex"
```

`program.ps1` 默认总是重新构建，避免误烧旧位流。只有明确需要烧录上一次
构建结果时才使用 `-SkipBuild`。

对于仓库中的示例，构建脚本还会比较 `main.c` 与 `build/firmware.hex`
的修改时间。如果 C 源码更新但 XC8 固件没有重编译，FPGA 构建会停止，
避免把旧 HEX 再次封装进新位流。

如果当前终端是 WSL/bash，不能直接运行 `.\xxx.ps1`。应打开 Windows
PowerShell，或者从 WSL 显式调用 `powershell.exe`。

> openFPGALoader 使用 JTAG Interface 0。若无法识别下载器，请按 Lushay
> Labs 教程用 Zadig 将 Interface 0 设置为 WinUSB，不要修改 UART
> Interface 1。
