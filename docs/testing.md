# 测试、功能范围与限制

## 测试入口

```sh
make unit-test       # PIC核心、外设和电路网络，不依赖XC8
make integration-test # 构建示例并验证命令行与Web C后端
make test            # 执行以上测试
```

`make test` 需要 GCC、MPLAB XC8 和
`PIC10-12Fxxx_DFP`。只修改核心代码且本机没有XC8时，可以先运行
`make unit-test`。

STM32和FPGA拥有额外测试入口：

```sh
make stm32-host-check
make stm32-host-test EXAMPLE=button
```

```powershell
.\fpga\scripts\simulate.ps1
```

## 端到端示例验证

集成测试会构建并执行真实XC8固件：

- `blink`：检查GP0定时翻转；
- `breathing_led`：检查GP0软件PWM边沿和占空比变化；
- `button`：向GP3注入按键事件，检查LED和蜂鸣器输出；
- `buzzer`：向GP3注入按键事件，检查GP2脉冲；
- `led_chaser`：检查GP0～GP2流水输出顺序；
- `playmusic`：检查GP2持续产生音乐方波边沿。

Web后端测试还会验证加载、状态读取、单步、运行和断点协议。

## 软件模拟器已实现

- PIC10F200/PIC10F202 Baseline 12位指令执行；
- W、PC、STATUS、FSR、PCL、OSCCAL和两级硬件栈；
- 型号对应的程序空间、RAM和INDF/FSR间接寻址；
- 跳转、调用、返回、计算跳转、跳过指令和一/双周期计时；
- GPIO/TRIS、GP3仅输入、读-修改-写、弱上拉和外部引脚驱动；
- Timer0内部/外部时钟、边沿、预分频和写入抑制；
- WDT、Sleep、POR、MCLR、引脚变化唤醒和WDT复位；
- Intel HEX和配置字加载；
- 反汇编、跟踪、断点、转储、GPIO事件和VCD；
- JSON电路网络、LED、按键、蜂鸣器及多MCU；
- Web拖放、连线、框选、多选移动、批量删除、保存、断点和状态面板；
- STM32 GPIO桥接和实时调度。

## 当前限制

- PIC10F204/206的模拟比较器尚未实现；
- 不模拟电压、温度、功耗、振荡误差和其他模拟电气特性；
- MCLR尚未作为完整异步电路端点；
- 没有ELF/DWARF源码级调试、条件断点和数据观察点；
- Web尚未提供电阻、电源、显示屏、串行总线等复杂器件；
- JSON连接的第四项走线指令仍为预留字段，不能保存自定义折线路径；
- FPGA核心是功能子集，不能用软件模拟器的完整功能列表判断RTL支持程度。

PIC传统指令周期频率为 `Fosc/4`。内部振荡器为4 MHz时，一个指令周期约为
1微秒；一条指令可能消耗一个或两个指令周期。
