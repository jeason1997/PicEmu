# 命令行模拟器

## 构建与运行

```sh
make
./build/picemu firmware.hex --cycles 100000
```

## 参数

```text
--cycles N          最多执行N个PIC指令周期
--device NAME       PIC10F200（默认）或PIC10F202
--quiet             不打印GPIO变化
--trace             逐条显示机器码、反汇编和寄存器
--disassemble       反汇编后退出
--break ADDRESS     执行到指定PC前停止
--dump              显示寄存器、硬件栈和RAM
--vcd FILE          生成GTKWave波形
--events FILE       按周期向GPIO注入外部事件
```

示例：

```sh
./build/picemu examples/blink/build/firmware.hex --disassemble
./build/picemu examples/playmusic/build/firmware.hex \
  --cycles 1100000
./build/picemu examples/blink/build/firmware.hex --cycles 100 --trace --dump
./build/picemu examples/blink/build/firmware.hex --break 0x00E --dump
./build/picemu examples/blink/build/firmware.hex \
  --cycles 10000 --vcd build/blink.vcd
gtkwave build/blink.vcd
```

VCD包含PC、W、STATUS、TMR0、GPIO总线、`gp0`～`gp3`和Sleep状态。
时间单位按PIC指令周期换算；默认4 MHz振荡器对应1 MHz指令周期，因此
一个周期在VCD中是1微秒。

`--trace`显示的是指令执行前的PC和寄存器状态。软件延时常由`DECFSZ`与
`GOTO`组成，逐条跟踪时在两条地址间重复属于正常执行。

## 外部引脚事件

事件文件格式：

```text
# cycle pin value
0 GP3 1
1000 GP3 0
80000 GP3 1
```

电平可以是 `0`、`1` 或 `z`，`z`表示释放外部驱动。事件在PIC指令边界
生效；如果目标周期位于两周期指令中间，会在该指令结束后应用。

完整固件示例和推荐运行入口见[示例说明](examples.md)。
