# Web 电路仿真器

Web 前端提供一个类似 SimulIDE 的本地电路实验台。它没有重新实现一套 PIC
解释器，而是通过本地服务调用项目现有的 C 核心，因此命令行、SDL 和 Web
执行的是同一套指令逻辑。

## 启动

需要 Windows 上安装 Node.js，并且 WSL 中能够使用 GCC 和 XC8。在 PowerShell
的项目根目录执行：

```powershell
.\frontends\web\scripts\start.ps1
```

启动时会构建 Web 后端和全部示例固件，然后访问
`http://127.0.0.1:4173`。可以指定初始示例和端口：

```powershell
.\frontends\web\scripts\start.ps1 -Example breathing_led -Port 8080
```

Linux 是启动流程的主入口：

```sh
./frontends/web/scripts/start.sh --example button --port 4173
```

也可执行 `make run-web EXAMPLE=button`。PowerShell 脚本只把参数转交给这个
Linux 脚本，不再单独维护构建和启动步骤。

## 使用

- 从左侧器件库把 PIC10F200、LED、按键或蜂鸣器拖到画布。
- 点击一个引脚，再点击另一个引脚完成连线。
- 拖动器件可以调整位置；右侧“器件”页可修改 ID 和 LED 颜色。
- 鼠标滚轮以指针位置为中心缩放，中键按住可平移整个画布。
- 器件坐标和布线拐点以半格（10 px）为吸附步长。
- “适应”会自动缩放并居中全部器件；打开电路时也会自动执行一次。
- 单击连线后可用“删除选中”或 `Delete` 删除，`Esc` 可取消正在进行的连线。
- 按键既可以按住产生输入，也可以像其他器件一样拖动。
- “设置 HEX”会把本地固件加载到当前 PIC。
- 运行、暂停、单步和复位按钮控制 CPU。
- 右侧面板实时显示寄存器、状态位、硬件栈、RAM，以及带反汇编文本的程序
  Flash；当前 PC 对应的指令会高亮。
- 按住画布上的按键会向连接的 PIC GPIO 输入低电平。
- “导出 JSON”保存的文件沿用 SDL 的 `diagram.json` 格式。

## 与 SDL 共用电路文件

Web 直接读取 `examples/*/diagram.json`，主要字段保持一致：

```json
{
  "version": 1,
  "clockHz": 4000000,
  "firmware": "examples/button/build/firmware.hex",
  "parts": [],
  "connections": []
}
```

器件坐标、属性和连接都会原样保留。Web 导出的 JSON 可作为 SDL 电路配置继续
使用；当前 SDL 不认识的纯界面属性应放入器件的 `attrs` 对象中。

## 架构

```text
浏览器编辑器
    │ HTTP / JSON
本地 Node 服务
    │ stdin / stdout
picemu-web-core（C）
    │
PIC10 CPU、HEX 加载器、器件描述
```

Node 服务只监听 `127.0.0.1`。用户上传的 HEX 暂存在
`build/web/uploads/firmware.hex`，执行 `make clean` 会将其删除。

## 当前范围

首版支持 PIC10F200、LED、按键和无源蜂鸣器。连线模型目前针对“一根 GPIO
连接一个简单外设”，尚未实现电阻、电源网络、模拟电压、总线和复杂电气冲突。
新增芯片或器件时，应分别扩展共享仿真设备层和 Web 的外观/交互层，不应在页面
里加入针对某个示例固件的特殊逻辑。
