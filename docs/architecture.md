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
├── examples/
│   ├── blink/             每个示例自带源码、diagram.json和构建目录
│   ├── button/
│   └── buzzer/
├── docs/
│   └── ports/             非桌面平台的移植说明
├── tests/                 单元测试与示例集成测试
└── tools/                 HEX转C等开发工具
```

## 依赖方向

```text
PIC核心 <- 仿真网络 <- SDL电路装配 <- SDL应用
   ^          ^
HEX映像     虚拟器件
   ^
HEX加载器 / 嵌入式C数组
```

- CPU核心不知道SDL、窗口、LED或JSON。
- 虚拟器件只通过引脚驱动和观察网络，不直接访问CPU内部状态。
- SDL应用不写死器件和连线，电路由JSON决定。
- STM32桥接不依赖SDL，可把虚拟PIC引脚映射到真实GPIO。

## 增加芯片型号

1. 在 `core` 增加型号描述和差异外设；
2. 尽量复用 Baseline CPU 指令执行器；
3. 在 SDL `parts` 增加对应封装外观；
4. 在电路装配器注册新的 `type`；
5. 增加该型号独立的示例固件与单元测试。

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
