# PicEmu 文档导航

本文档目录按“先使用、再理解、最后移植和扩展”的顺序组织。

## 入门

- [项目主页](../README.md)：功能总览、快速开始和目录结构。
- [PIC10F200/PIC10F202](pic10f200.md)：封装、引脚、TRISGPIO 和型号差异。
- [示例说明](examples.md)：每个固件示例的用途、外设和平台兼容性。
- [编译 PIC 固件](firmware.md)：在 Linux/WSL 中用 XC8 和 DFP 生成 HEX。

## 桌面仿真

- [命令行模拟器](cli.md)：执行、反汇编、跟踪、断点、事件和 VCD。
- [SDL 电路仿真器](sdl-circuits.md)：运行 JSON 电路、按键/蜂鸣器交互和
  电路配置格式。
- [Web 电路实验台](web-simulator.md)：可视编辑、调试面板、多 MCU 和保存。

## 端口与硬件

- [STM32F103 端口](ports/stm32f103.md)：时钟、GPIO 映射、构建、日志和烧录。
- [Tang Nano 1K FPGA](../fpga/README.md)：RTL 核心、BSRAM 程序存储器、
  OSS-CAD-Suite 构建和烧录。

## 开发与维护

- [项目架构](architecture.md)：模块边界、依赖方向以及增加芯片/器件的方法。
- [测试与功能范围](testing.md)：测试命令、已实现特性和当前限制。

## 文档维护约定

- 主 README 只保留项目总览和最短上手路径，细节放入对应主题文档。
- 命令应从项目根目录执行，例外情况会明确写出 `cd`。
- 生成文件统一位于 `build/` 或模块自己的 `build/`，不纳入版本控制。
- 代码注释、文档和 Git 提交说明统一使用中文；命令、标识符和标准名称保留
  原始英文。
