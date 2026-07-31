# Tang Nano 1K FPGA 规则

- FPGA 代码按 RTL、约束、脚本、工具和测试平台划分在 `fpga/` 中。
- 使用 Windows 下的 OSS-CAD-Suite 命令行构建和烧录，不依赖官方 IDE。
- FPGA 实现独立的 Verilog PIC10F200 核心，不调用软件 C 模拟器。
- PIC 程序 ROM 随位流写入 FPGA 配置 Flash，运行时使用片上 BSRAM 取指，不浪费
  LUT 表示程序存储器。
- 更换 HEX 后需要重新生成并烧录位流；文档必须明确 CPU 核心、ROM 初始化文件、
  配置 Flash 和运行时 BSRAM 的关系。
- 测试平台只验证通用 CPU 行为，不保留针对流水灯等示例的特殊 RTL。
- 修改 RTL 后应执行命令行构建。用户要求实板验证时，应自动烧录并依据用户观察
  继续定位，不能只声称综合通过。
