# SDL界面与JSON电路配置

## 构建与运行

```sh
sudo apt install libsdl2-dev
make sdl
make firmware
./build/picemu-sdl examples/blink/diagram.json
```

命令行第二个参数可覆盖JSON中的固件：

```sh
./build/picemu-sdl examples/blink/diagram.json another.hex
```

操作：

- 调整窗口大小：画面等比例缩放；
- 鼠标滚轮：以指针位置为中心缩放画布；
- 按住鼠标中键拖动：向任意方向平移画布；
- `+`/`-`：放大或缩小，`0`：恢复100%与初始位置；
- 鼠标按住虚拟按键：向相连引脚施加有效电平；
- `Space`：运行或暂停；
- `N`：暂停时单步；
- `R`：复位；
- `Esc`：退出。

`N` 是精确的指令级单步。编译器生成的软件延时通常由 `DECFSZ` 和
`GOTO` 构成循环，因此单步时会在这两条指令间重复很多次。这是固件的真实
执行流程，并不表示PC卡死。

## 配置格式

格式参考Wokwi的 `diagram.json` 思路，目前实现一个明确的子集：

```json
{
  "version": 1,
  "clockHz": 4000000,
  "firmware": "examples/blink/build/firmware.hex",
  "parts": [
    {
      "id": "mcu",
      "type": "pic10f200",
      "left": 330,
      "top": 150,
      "attrs": {
        "firmware": "examples/blink/build/firmware.hex"
      }
    },
    {
      "id": "led0",
      "type": "led",
      "left": 150,
      "top": 200,
      "attrs": { "color": "red" }
    }
  ],
  "connections": [
    ["mcu:GP0", "led0:A", "red", []]
  ]
}
```

`left`和`top`使用960×600逻辑画布坐标。连接端点格式为
`器件ID:引脚名`。

`clockHz`表示主控振荡器频率，PIC10的指令周期频率按`clockHz / 4`
计算。省略时默认使用4 MHz。这个时基同时用于实时执行速度、外设推进和
蜂鸣器频率测量。

电路可以包含多颗 `pic10f200`/`pic10f202`，不设置独立的主控数量限制。
每颗主控拥有独立的CPU、寄存器、RAM和程序存储器，并通过自己的
`attrs.firmware` 指定HEX；顶层 `firmware` 仅作为旧版单主控电路的兼容
回退。主控之间也可以像普通器件一样通过GPIO连线。

| type | 引脚 | attrs |
|---|---|---|
| `pic10f200` | GP0、GP1、GP2、GP3；VDD/VSS仅显示 | firmware |
| `pic10f202` | 与PIC10F200引脚兼容 | firmware |
| `led` | A或IN | color: red/green/blue/yellow |
| `pushbutton` | 1或OUT | activeLow: true/false |
| `buzzer` | 1或IN | 无 |

连接数组第三项是导线颜色。第四项为未来的走线指令预留，目前忽略。
同一个端点出现在多条连接中时，这些连接会自动合并为一个公共网络，因此
可以把一个GPIO同时连接到多个外设，也可以先连接外设再连接主控。
完整示例见：

- [`examples/blink/`](../examples/blink/)
- [`examples/breathing_led/`](../examples/breathing_led/)：GP0软件PWM红色呼吸灯
- [`examples/button/`](../examples/button/)
- [`examples/buzzer/`](../examples/buzzer/)
- [`examples/playmusic/`](../examples/playmusic/)：PIC10F200 软件方波播放简单旋律

`buzzer`既支持固定高电平驱动的有源蜂鸣器，也会测量GPIO方波的边沿周期。
`playmusic`示例利用GP2软件翻转产生不同频率，SDL音频会按测得频率发声。

## 增加可视器件

1. 在 `src/sim/devices/` 增加独立电气模型；
2. 在 `frontends/sdl/parts/` 增加外观、引脚位置和交互；
3. 在 `frontends/sdl/parts/registry.c` 注册JSON的 `type`；
4. 在 `examples/<名称>/` 放置 `main.c`、`diagram.json` 和 Makefile；
5. 增加相应集成测试。

SDL主循环不需要知道新器件的坐标、引脚或内部行为。

`attrs`由通用键值表保存，解析器不认识也不需要认识`color`、
`activeLow`等器件专用属性。每个器件从自己的初始化函数读取所需字段，
因此增加新属性不需要修改公共`CircuitPartConfig`。

Web编辑器也读写这份格式，字段兼容规则见
[Web与SDL共用电路文件](web-simulator.md#与-sdl-共用电路文件)。
