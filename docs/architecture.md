# 项目结构与模块边界

工程按职责划分目录，公开头文件使用 `picemu/...` 命名空间，构建目录会
镜像源目录结构，避免所有目标文件堆在同一层。

```text
PicEmu/
├── include/picemu/
│   ├── core/              CPU、芯片描述、反汇编公开接口
│   ├── firmware/          固件映像与HEX加载接口
│   ├── sim/               电路网络、设备和JSON配置接口
│   ├── platform/          真实硬件GPIO桥接接口
│   └── cli/               命令行专用接口
├── src/
│   ├── core/              PIC核心实现
│   ├── firmware/          Intel HEX加载器
│   ├── sim/
│   │   ├── config/        电路配置解析
│   │   └── devices/       LED、按键、蜂鸣器模型
│   ├── platform/          平台桥接实现
│   └── cli/               命令行程序和VCD输出
├── frontends/sdl/
│   ├── app/               SDL应用入口、事件和时间调度
│   ├── circuit/           配置到运行时电路的装配
│   ├── parts/             各器件独立可视模块
│   └── common/            位图文字等公共绘图代码
├── frontends/web/
│   ├── public/
│   │   ├── devices/       各器件的元数据、引脚和可视模板
│   │   └── app.js         通用画布、连线、调试和状态调度
│   ├── backend/           面向Web的C调试协议进程
│   ├── scripts/           Linux主启动脚本和PowerShell包装
│   └── server.mjs         静态文件、保存接口和多MCU进程管理
├── examples/
│   └── */                 每个示例自带源码、diagram.json和构建目录
├── ports/stm32f103/       裸机端口、链接脚本和烧录包装
├── fpga/
│   ├── rtl/               独立Verilog PIC10F200实现
│   ├── boards/            Tang Nano 1K顶层与引脚约束
│   ├── tb/                RTL测试平台
│   └── scripts/           OSS-CAD-Suite构建、仿真和烧录
├── docs/
│   └── ports/             非桌面平台的移植说明
├── tests/                 单元测试与示例集成测试
└── docs/                  使用、架构、测试和移植文档
```

## 依赖方向

```text
PIC核心 -> SimMcu适配器 -> 仿真网络 <- 虚拟器件
   ^                         ^
HEX映像                 SDL电路装配 <- SDL应用
   ^
HEX加载器 / 嵌入式C数组

浏览器 <-> Node本地服务 <-> 每颗MCU一个Web C后端进程

XC8 HEX -> HEX转C -> STM32裸机程序
XC8 HEX -> HEX转MEM -> FPGA BSRAM初值和位流
```

- CPU核心不知道SDL、窗口、LED或JSON。
- 虚拟器件只通过引脚驱动和观察网络，不直接访问CPU内部状态。
- SDL应用不写死器件和连线，电路由JSON决定。
- Web负责编辑和调试交互，PIC指令仍由C后端执行，不在JavaScript中复制。
- STM32桥接不依赖SDL，可把虚拟PIC引脚映射到真实GPIO。
- FPGA是独立RTL实现，不链接C模拟器；它通过相同XC8 HEX验证兼容行为。

`SimBoard`只依赖`SimMcu`接口，不再保存`Pic10Cpu *`。PIC10通过
`SimPic10Mcu`适配器提供复位、执行、引脚驱动和停止状态；后续PIC12、
PIC16或其他架构可提供自己的适配器而无需修改网络层。

电路容量统一定义在`include/picemu/sim/limits.h`。网络支持多个端点，
SDL装配器会根据重复端点自动合并连接，不再限制为“一个主控引脚连接一个
外设”。电路允许多颗MCU，每颗MCU拥有独立CPU和固件映像。

## 固件在各平台的流向

| 平台 | 构建时 | 运行时 |
|---|---|---|
| 命令行/SDL | 从文件系统读取Intel HEX | `HexImage`保存在进程内存 |
| Web | Node按MCU启动C后端并传入HEX路径 | 每个后端保存独立`Pic10Cpu` |
| STM32F103 | `hex2c`生成C常量并链接进固件 | 从STM32 Flash中的常量初始化CPU |
| Tang Nano 1K | HEX转为12位MEM并写入位流 | FPGA配置时初始化片上BSRAM |

这四条路径共用XC8生成的固件格式，但FPGA的CPU执行器是Verilog版本，其
实现范围应单独参考FPGA文档。

## 增加芯片型号

1. 在 `core` 增加型号描述和差异外设；
2. 尽量复用 Baseline CPU 指令执行器；
3. 在 SDL `parts` 增加对应封装外观并注册新的 `type`；
4. 在 Web 增加器件定义、外观和属性面板；
5. 判断STM32通用适配器是否需要型号参数；
6. FPGA若要支持该型号，需要独立扩展RTL，不能假设C代码会自动生效；
7. 增加该型号的真实XC8固件、单元测试和前端测试。

目前设备查找器接受PIC10F200和PIC10F202。PIC10F204/206仍未支持。

## 仿真设备头文件

`include/picemu/sim/device.h` 只定义所有设备共用的引脚电平、
`SimDevice` 和 `SimDeviceOps`，不包含任何具体器件。

每种器件拥有独立公开接口：

```text
include/picemu/sim/devices/
├── led.h
├── button.h
└── buzzer.h
```

设备实现只包含自己的头文件。使用者也应只包含实际需要的设备头文件；
新增屏幕或电机等器件时，不需要修改基础 `device.h`。

SDL 可视化器件采用相同原则：

```text
frontends/sdl/parts/
├── part.h / part.c          通用视图接口与分派
├── registry.h / registry.c JSON类型到初始化函数的注册表
├── led.h / led.c
├── button.h / button.c
├── buzzer.h / buzzer.c
└── pic10.h / pic10.c
```

`SdlPart` 只保存通用操作表、模型指针和视图状态，不再包含具体器件枚举或
设备 `union`。绘制、引脚查询、鼠标输入和音频输出均通过 `SdlPartOps`
分派。新增 SDL 器件无需修改 `part.h` 或电路装配器，只需添加独立模块并
在 `registry.c` 的内建设备表中登记一次。
