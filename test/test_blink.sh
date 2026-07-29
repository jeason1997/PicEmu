#!/bin/sh
#
# 按键固件端到端测试：
# XC8生成HEX -> 注入GP3按键事件 -> 验证两颗LED反转及蜂鸣器脉冲。

set -eu

PICEMU=${1:-../build/picemu}
FIRMWARE=${2:-build/blink.hex}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

OUTPUT=$("$PICEMU" "$FIRMWARE" --cycles 120000 \
    --events "$SCRIPT_DIR/button_events.txt")
printf '%s\n' "$OUTPUT"

# 初始LED状态应为GP0=1。按键按下并经过20ms消抖后：
# GP0变0、GP1变1、GP2变1；约50ms后GP2恢复0。
printf '%s\n' "$OUTPUT" | awk '
/\] GP0 -> 1$/ { initial_led1 = 1 }
/\] GP0 -> 0$/ { led1_inverted = 1 }
/\] GP1 -> 1$/ { led2_inverted = 1 }
/\] GP2 -> 1$/ {
    buzzer_on = 1
    line = $0
    sub(/^\[[^0-9]*/, "", line)
    sub(/\].*$/, "", line)
    buzzer_on_cycle = line + 0
}
/\] GP2 -> 0$/ {
    buzzer_off = 1
    line = $0
    sub(/^\[[^0-9]*/, "", line)
    sub(/\].*$/, "", line)
    buzzer_off_cycle = line + 0
}
END {
    if (!initial_led1 || !led1_inverted || !led2_inverted) {
        print "测试失败：按键后两颗LED没有正确反转" > "/dev/stderr"
        exit 1
    }
    if (!buzzer_on || !buzzer_off) {
        print "测试失败：没有观察到完整蜂鸣器脉冲" > "/dev/stderr"
        exit 1
    }
    duration = buzzer_off_cycle - buzzer_on_cycle
    if (duration < 49980 || duration > 50020) {
        printf "测试失败：蜂鸣器持续%d周期，不是约50ms\n", duration > "/dev/stderr"
        exit 1
    }
    printf "测试通过：按键后LED1/LED2反转，蜂鸣器持续%d周期。\n",
           duration
}
'
