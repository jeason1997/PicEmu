# PIC10F200 教学模拟器

这是一个使用 C11 编写的 PIC10F200 模拟器，可以在 Linux 下通过 GCC
编译，加载 MPLAB XC8 生成的 Intel HEX 固件并执行。

项目强调结构清晰和中文注释，适合作为 MCU 指令集模拟器的入门示例，
不以替代 MPLAB SIM 为目标。

## 项目结构

```text
PicEmu/
├── include/
│   ├── hex_loader.h       Intel HEX 加载器接口
│   └── pic10f200.h        PIC10F200 模拟器接口和状态定义
├── src/
│   ├── hex_loader.c       Intel HEX 解析与校验
│   ├── pic10f200.c        CPU、RAM、栈、GPIO、Timer0
│   └── main.c             命令行入口和执行循环
├── test/
│   ├── Makefile           测试固件编译、运行和验证
│   ├── blink.c            XC8 闪灯示例
│   └── test_blink.sh      端到端自动测试
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

命令行参数：

```text
--cycles N  最多执行 N 个 PIC 指令周期，默认 1000000
--quiet     不逐条打印 GPIO 变化，只显示最终摘要
```

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
- 内部时钟源下的 Timer0 和预分频器。
- Intel HEX 数据、EOF、扩展段地址及扩展线性地址记录。
- MPLAB/XC8 放在字节地址 `0x1FFE` 的配置字。

## 当前限制

为了保持第一版适合学习，目前没有完整模拟：

- WDT 超时与复位
- MCLR 和外部引脚事件调度
- T0CKI 外部时钟输入
- 振荡器误差与电气特性
- ELF/DWARF 源代码级调试

模拟器使用“PIC 指令周期”作为时间单位。传统 PIC 的指令周期频率通常为
`Fosc / 4`；若内部振荡器为 4 MHz，一个指令周期约为 1 微秒。
