# PY32F002A 端口规则

- 端口统一位于 `ports/py32f002a/`，器件支持文件与端口放在一起。
- 默认引脚映射为虚拟 GP0 → PB1；所有映射必须集中在 `include/board_config.h`。
- 所选示例通过 `EXAMPLE` 构建参数指定，不得为某个 PIC 固件加入特殊逻辑。
- 系统时钟固定使用参考工程验证过的 48 MHz HSI + PLL 配置；实时同步参数必须由
  实际系统时钟推导。
- 烧录默认使用 CMSIS-DAP 和 OpenOCD，并允许通过环境变量覆盖工具与接口配置。
- Windows 烧录由 `scripts/program.ps1` 负责参数转换和调用统一构建入口，不复制
  Makefile 中的编译规则。
- Windows OpenOCD 不提交到仓库；脚本从普冉官方固定版本下载、校验后放入被
  Git 忽略的本地工具缓存。
