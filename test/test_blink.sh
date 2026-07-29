#!/bin/sh
#
# blink.hex 端到端测试：
#   XC8 生成 HEX -> picemu 加载 HEX -> 检查 GP0 翻转结果。
#
# 此脚本既可由 test/Makefile 调用，也可以手动执行：
#   sh test/test_blink.sh ../build/picemu build/blink.hex

set -eu

PICEMU=${1:-../build/picemu}
FIRMWARE=${2:-build/blink.hex}

OUTPUT=$("$PICEMU" "$FIRMWARE" --cycles 3500000)
printf '%s\n' "$OUTPUT"

# 模拟器只打印真正发生电平变化的引脚。这里不依赖“周期”两个中文字，
# 避免不同 shell locale 影响测试，只提取方括号内的数字以及 GP0 电平。
EVENTS=$(printf '%s\n' "$OUTPUT" |
    sed -n 's/^\[[^0-9]*\([0-9][0-9]*\)\] GP0 -> \([01]\)$/\1 \2/p')

printf '%s\n' "$EVENTS" | awk '
BEGIN {
    expected[1] = 1
    expected[2] = 0
    expected[3] = 1
    expected[4] = 0
}
{
    count++
    cycle[count] = $1
    level[count] = $2
}
END {
    if (count < 4) {
        print "测试失败：没有观察到至少 4 次 GP0 翻转" > "/dev/stderr"
        exit 1
    }

    for (i = 1; i <= 4; ++i) {
        if (level[i] != expected[i]) {
            print "测试失败：GP0 电平序列不是 1,0,1,0" > "/dev/stderr"
            exit 1
        }
    }

    interval1 = cycle[3] - cycle[2]
    interval2 = cycle[4] - cycle[3]
    difference = interval1 - interval2
    if (difference < 0) {
        difference = -difference
    }

    # 两段 delay(50000) 理应基本相等。函数入口、循环跳转和连续 GPIO
    # 位操作可能造成几个指令周期的固定差异，因此允许最多 8 个周期。
    if (difference > 8) {
        print "测试失败：高、低电平延时间隔不一致" > "/dev/stderr"
        exit 1
    }

    printf "测试通过：GP0 按 1,0,1,0 翻转，稳态间隔为 %d/%d 个周期。\n",
           interval1, interval2
}
'
