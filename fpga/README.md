# Tang Nano 1K FPGA 实现

这里用于逐步在 Tang Nano 1K（GW1NZ-LV1QN48C6/I5）上实现
PIC10F200。当前第一阶段是验证 Windows 下的开源 FPGA 工具链、板级时钟、
按键和 LED 引脚，尚未包含 PIC CPU 核心。

## 目录

```text
fpga/
├── boards/tang_nano_1k/       # 开发板引脚与电气约束
├── examples/button_toggle/    # BTN1 切换 LED1 的首个实板示例
├── rtl/common/                # 与具体示例无关的可复用 RTL
├── scripts/                   # Windows PowerShell 构建、仿真、烧录脚本
└── build/                     # 自动生成，不提交 Git
```

## 示例行为

- BTN1（物理 13 脚）：按下一次，LED1 切换一次。
- BTN2（物理 44 脚）：复位，LED1 熄灭。
- LED1（物理 9 脚）：低电平点亮。
- LED2、LED3：本示例保持熄灭。

BTN1 经过两级同步和约 20 ms 消抖。一直按住不会连续切换，必须先释放再按下。

约束中的 I/O 标准统一使用 Sipeed 官方 Tang Nano 1K 示例所采用的
`LVCMOS33`。本机旧参考工程只使用了 LED，却把 LED/时钟写成 `LVCMOS18`；
加入同一 Bank 的按键后会被 Apicula 正确拒绝，因此这里没有照搬该项设置。

## Windows 命令行

脚本默认查找 `E:\oss-cad-suite`。如果安装在其他位置，可以先设置：

```powershell
$env:OSS_CAD_SUITE = "D:\tools\oss-cad-suite"
```

在仓库根目录执行逻辑仿真：

```powershell
.\fpga\scripts\simulate.ps1
```

执行综合、布局布线并生成位流：

```powershell
.\fpga\scripts\build.ps1
```

连接开发板后，将位流下载到 SRAM：

```powershell
.\fpga\scripts\program.ps1
```

输出位流位于 `fpga\build\button_toggle\button_toggle.fs`。

也可以在 VS Code 中打开
`examples/button_toggle/button_toggle.lushay.json`，交给 Lushay Code 插件构建。

> 如果 openFPGALoader 找不到下载器，需要按照 Lushay Labs 教程，用 Zadig
> 只把设备的 JTAG Interface 0 驱动改为 WinUSB；UART Interface 1 不要修改。
