# PicEmu

PicEmu 是一个使用 C11 编写的 PIC10F200/PIC10F202 教学模拟器，可在Linux下通过
GCC 编译，直接执行 MPLAB XC8 生成的 Intel HEX 固件。

项目包含两个入口：

- `build/picemu`：命令行执行、反汇编、断点、引脚事件和 VCD 波形；
- `build/picemu-sdl`：由 JSON 配置电路的 SDL2 交互式仿真界面。

## 快速开始

```sh
# 构建命令行模拟器
make

# 构建全部XC8示例固件并运行自动测试
make test

# 构建并运行button示例的SDL电路
sudo apt install libsdl2-dev
make run-sdl

# 选择其他示例
make run-sdl EXAMPLE=blink
make run-sdl EXAMPLE=buzzer
make run-sdl EXAMPLE=playmusic
```

手动运行：

```sh
./build/picemu examples/button/build/firmware.hex \
  --cycles 90000 --events examples/button/events.txt

./build/picemu-sdl examples/button/diagram.json
```

## 文档

- [项目结构与模块边界](docs/architecture.md)
- [PIC10F200引脚与功能](docs/pic10f200.md)
- [命令行模拟器](docs/cli.md)
- [SDL界面与JSON电路配置](docs/sdl-circuits.md)
- [在Linux下编译PIC固件](docs/firmware.md)
- [测试、功能范围与限制](docs/testing.md)
- [移植到STM32F103](docs/ports/stm32f103.md)

## 当前状态

目前实现 PIC10F200 和 PIC10F202。两者共享Baseline CPU核心，由设备描述
决定程序空间、RAM映射和PC宽度。PIC10F204/206的比较器尚未实现。
