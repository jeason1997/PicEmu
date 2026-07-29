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
make -C test firmware \
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

## 嵌入固件

没有文件系统的平台可先把HEX转换成C常量：

```sh
make tools
./build/hex2c examples/blink/build/firmware.hex blink > blink_firmware.c
```

生成文件提供 `const HexImage blink_image`，可以直接传给
`pic10f200_init()`。
