# 在Linux下编译PIC固件

测试固件使用 Microchip MPLAB XC8，不需要启动 MPLAB X IDE。
PIC10F200还需要 `PIC10-12Fxxx_DFP` Device Family Pack。

常见安装位置：

```text
/opt/microchip/xc8/v4.00/bin/xc8-cc
~/.mchp_packs/Microchip/PIC10-12Fxxx_DFP/<版本>/xc8
```

构建：

```sh
make firmware
```

如果DFP不在自动搜索位置：

```sh
make -C examples/blink firmware \
  DFP=/path/to/PIC10-12Fxxx_DFP/version/xc8
```

每个示例的XC8产物位于自己的 `build/`，例如：

```text
examples/blink/build/firmware.hex
examples/button/build/firmware.hex
examples/buzzer/build/firmware.hex
```

构建单个示例：

```sh
make -C examples/blink
```

Windows PowerShell 可以把同一条构建命令交给 WSL，例如：

```powershell
wsl.exe --cd E:\Projects\PicEmu bash -ic `
  "make -C examples/seven_segment firmware"
```

`-i` 会加载用户在 `.bashrc` 中配置的 XC8 环境。构建规则优先读取 `XC8`
和 `PIC10_DFP` 环境变量，未设置 `XC8` 时从 `PATH` 查找 `xc8-cc`；
仍可通过 `XC8=`、`DFP=` 显式覆盖。

所有示例及用途见[示例说明](examples.md)。

## 嵌入式平台

STM32F103端口会在自身构建过程中自动编译私有的HEX转C工具，把所选固件
生成成`const HexImage pic_firmware_image`并链接进STM32程序。该工具属于
STM32构建实现，位于`ports/stm32f103/tools/`，普通桌面构建不需要它。

FPGA使用另一条独立流程，把HEX转换成12位MEM文件并作为BSRAM初值写入
位流。两种方式详见[STM32F103端口](ports/stm32f103.md)和
[Tang Nano 1K FPGA](../fpga/README.md)。
