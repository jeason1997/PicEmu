# Tang Nano 1K 上的 PIC10F200

本目录使用 Verilog 在 Tang Nano 1K（GW1NZ-LV1QN48C6/I5）中实现
PIC10F200。XC8 生成的 Intel HEX 会在构建期间转换为 12 位程序镜像，
与 PIC CPU 核心一起封装进 FPGA 位流。

当前实现不是把示例行为写死在 RTL 中。更换 `-Firmware` 参数并重新构建
位流，就能运行另一个符合当前指令集和容量限制的 PIC 固件。

## CPU 核心与 ROM 存在哪里

系统中有三种不同的存储概念：

| 内容 | 断电时存放位置 | 运行时位置 |
|---|---|---|
| PIC10F200 CPU 核心 | GW1NZ-1 内部配置 Flash 的位流 | FPGA 逻辑单元和寄存器 |
| PIC 固件/ROM | 同一个内部配置 Flash 位流 | 1 个片上 BSRAM（SPX9） |
| PIC 数据寄存器 | 不持久化 | FPGA 寄存器 |

GW1NZ-1 的内部配置 Flash 不是用户逻辑能够按地址读取的普通存储器，因此
不存在 PIC CPU 自己执行“从内部 Flash 复制 ROM”的软件循环。实际启动过程是：

```text
上电
  ↓
GW1NZ-1 配置电路读取内部配置 Flash
  ↓
建立 PIC CPU 逻辑，同时把 firmware.mem 初值装入 BSRAM
  ↓
FPGA 配置完成
  ↓
PIC CPU 从 BSRAM 地址 0 开始取指
```

这就是 FPGA 中“固件随位流保存、运行时位于 RAM”的标准实现。程序存储器
使用同步读模板并以 `ram_style = "block"` 声明；构建时还启用
`synth_gowin -nolutram`，防止退化成 LUTRAM。验证使用的资源报告为：

```text
SPX9:  1
BSRAM: 1 / 4 (25%)
```

如果工具链不能放置 BSRAM，构建会直接失败，不会静默改用 LUT ROM。

## 当前实现范围

当前阶段可运行 `blink` 和 `led_chaser` 等简单 XC8 固件：

- 12 位指令取指和译码；
- 8 位 W、PC 和 STATUS；
- GPIO 输出锁存器和 TRIS 方向控制；
- GP3 固定输入；
- 三个 GPR（`0x10`～`0x12`）；
- NOP、TRIS、MOVWF、CLRF、DECFSZ、GOTO、MOVLW；
- 跳转和成功跳过指令的第二个指令周期；
- 27 MHz FPGA 时钟产生 1 MHz PIC 指令周期；
- 64 个 12 位程序字的 BSRAM 程序窗口。

尚未实现全部 16 字节 RAM、完整指令集、两级栈、Timer0、WDT、SLEEP 和
外部 T0CKI。HEX 转换器会拒绝超过 64 字或包含未实现指令的固件，避免静默
执行错误。

## 引脚映射

| PIC10F200 | Tang Nano 1K | 功能 |
|---|---:|---|
| GP0 | pin 9 | 红色 LED |
| GP1 | pin 10 | 蓝色 LED |
| GP2 | pin 11 | 绿色 LED |
| GP3 | pin 13 / BTN1 | PIC 输入 |
| 复位 | pin 44 / BTN2 | FPGA/PIC 核心复位 |
| 时钟 | pin 47 | 板载 27 MHz |

板载 RGB LED 为低电平点亮，顶层模块已经处理该电气极性。PIC 固件仍按普通
逻辑编写：向 GPx 写 `1` 表示虚拟引脚输出高电平。

## 目录结构

```text
fpga/
├── boards/tang_nano_1k/       # 板级顶层和 CST 引脚约束
├── rtl/core/                  # PIC10F200 CPU 核心
├── rtl/memory/                # BSRAM 程序存储器
├── rtl/common/                # 时钟使能、输入同步等复用模块
├── tb/                        # 与具体固件无关的 RTL 测试
├── tools/                     # Intel HEX 转 12 位程序镜像
├── scripts/                   # Windows 构建、仿真和烧录脚本
├── pic10f200.lushay.json      # Lushay Code 工程入口
└── build/                     # 自动生成，不提交 Git
```

外置 P25Q16H 不参与当前架构，也没有任何外置 Flash 测试逻辑或引脚约束。

## 工具链要求

GW1NZ-1 芯片本身有 4 个 BSRAM，但旧版 nextpnr-gowin 数据库可能错误地把
可用 BSRAM 数量报告为 0，并出现：

```text
Unable to place cell ... no BELs remaining to implement cell type 'SPX9'
```

本项目已使用 OSS-CAD-Suite `2026-07-29` 的
`nextpnr-himbaechel` 验证通过。请使用该版本或更新版本。脚本默认从
`E:\oss-cad-suite` 加载，也可以设置：

```powershell
$env:OSS_CAD_SUITE = "D:\tools\oss-cad-suite"
```

## 构建与测试

运行与固件无关的 CPU 核心测试：

```powershell
.\fpga\scripts\simulate.ps1
```

构建默认 `blink`：

```powershell
.\fpga\scripts\build.ps1
```

构建三色流水灯：

```powershell
.\fpga\scripts\build.ps1 `
  -Firmware "examples\led_chaser\build\firmware.hex"
```

构建脚本会执行：

1. 校验 HEX 的容量和指令；
2. 生成 `build/pic10f200/firmware.mem`；
3. 综合 PIC 核心和 BSRAM；
4. 布局布线并生成包含 CPU 和固件的 `.fs` 位流。

## 烧录

默认写入 GW1NZ-1 内部配置 Flash，断电后仍会保留并在下次上电自动运行：

```powershell
.\fpga\scripts\program.ps1 `
  -Firmware "examples\led_chaser\build\firmware.hex"
```

仅做临时测试、只加载 FPGA SRAM：

```powershell
.\fpga\scripts\program.ps1 `
  -Firmware "examples\led_chaser\build\firmware.hex" `
  -Volatile
```

仅重新烧录上一次已生成的位流：

```powershell
.\fpga\scripts\program.ps1 -SkipBuild
```

更换 PIC 固件后必须重新生成位流，因为 PIC ROM 初值就是位流的一部分。
对于仓库示例，如果 `main.c` 比 `firmware.hex` 更新，构建会停止并要求先用
XC8 重编译，避免封装旧固件。

如果当前终端是 WSL/bash，不能直接运行 `.\xxx.ps1`。请打开 Windows
PowerShell，或从 WSL 显式调用 `powershell.exe`。

> openFPGALoader 使用 JTAG Interface 0。若无法识别下载器，请按 Lushay
> Labs 教程使用 Zadig 将 Interface 0 设置为 WinUSB，不要修改串口接口。
