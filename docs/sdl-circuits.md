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
- 鼠标按住虚拟按键：向相连引脚施加有效电平；
- `Space`：运行或暂停；
- `N`：暂停时单步；
- `R`：复位；
- `Esc`：退出。

## 配置格式

格式参考Wokwi的 `diagram.json` 思路，目前实现一个明确的子集：

```json
{
  "version": 1,
  "firmware": "examples/blink/build/firmware.hex",
  "parts": [
    {
      "id": "mcu",
      "type": "pic10f200",
      "left": 330,
      "top": 150
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

| type | 引脚 | attrs |
|---|---|---|
| `pic10f200` | GP0、GP1、GP2、GP3；VDD/VSS仅显示 | 无 |
| `pic10f202` | 与PIC10F200引脚兼容 | 无 |
| `led` | A或IN | color: red/green/blue/yellow |
| `pushbutton` | 1或OUT | activeLow: true/false |
| `buzzer` | 1或IN | 无 |

连接数组第三项是导线颜色。第四项为未来的走线指令预留，目前忽略。
完整示例见：

- [`examples/blink/`](../examples/blink/)
- [`examples/button/`](../examples/button/)
- [`examples/buzzer/`](../examples/buzzer/)
- [`examples/playmusic/`](../examples/playmusic/)：PIC10F202 软件方波播放较长的《致爱丽丝》主旋律

`buzzer`既支持固定高电平驱动的有源蜂鸣器，也会测量GPIO方波的边沿周期。
`playmusic`示例利用GP2软件翻转产生不同频率，SDL音频会按测得频率发声。

## 增加可视器件

1. 在 `src/sim/devices/` 增加独立电气模型；
2. 在 `frontends/sdl/parts/` 增加外观、引脚位置和交互；
3. 在 `sdl_circuit.c` 注册JSON的 `type`；
4. 在 `examples/<名称>/` 放置 `main.c`、`diagram.json` 和 Makefile；
5. 增加相应集成测试。

SDL主循环不需要知道新器件的坐标、引脚或内部行为。
