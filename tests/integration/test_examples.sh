#!/bin/sh

set -eu

PICEMU=${1:-build/picemu}

run_example()
{
    name=$1
    cycles=$2
    events=${3:-}
    firmware="examples/$name/build/firmware.hex"

    if [ -n "$events" ]; then
        "$PICEMU" "$firmware" --cycles "$cycles" \
            --events "examples/$name/$events"
    else
        "$PICEMU" "$firmware" --cycles "$cycles"
    fi
}

BLINK=$(run_example blink 2200000)
printf '%s\n' "$BLINK" | grep -q 'GP0 -> 1'
printf '%s\n' "$BLINK" | grep -q 'GP0 -> 0'
if printf '%s\n' "$BLINK" | grep -q 'GP1 ->'; then
    echo "blink示例不应改变GP1" >&2
    exit 1
fi

BREATHING=$(run_example breathing_led 300000)
BREATHING_EDGES=$(printf '%s\n' "$BREATHING" |
    grep -c 'GP0 ->' || true)
if [ "$BREATHING_EDGES" -lt 20 ]; then
    echo "breathing_led示例应在GP0产生连续的软件PWM" >&2
    exit 1
fi

BUTTON=$(run_example button 70000 events.txt)
printf '%s\n' "$BUTTON" | grep -q 'GP0 -> 0'
printf '%s\n' "$BUTTON" | grep -q 'GP1 -> 1'

BUZZER=$(run_example buzzer 120000 events.txt)
printf '%s\n' "$BUZZER" | grep -q 'GP2 -> 1'
printf '%s\n' "$BUZZER" | grep -q 'GP2 -> 0'

LED_CHASER=$(run_example led_chaser 400000)
LED_CHASER_RISES=$(printf '%s\n' "$LED_CHASER" |
    sed -n 's/.*\(GP[012] -> 1\).*/\1/p' |
    head -n 3)
if [ "$LED_CHASER_RISES" != "GP0 -> 1
GP1 -> 1
GP2 -> 1" ]; then
    echo "led_chaser示例的前三次点亮顺序应为GP0、GP1、GP2" >&2
    exit 1
fi

PLAYMUSIC=$(run_example playmusic 10000)
printf '%s\n' "$PLAYMUSIC" | grep -q 'PIC10F200'
printf '%s\n' "$PLAYMUSIC" | grep -q 'GP2 -> 1'
printf '%s\n' "$PLAYMUSIC" | grep -q 'GP2 -> 0'

echo "示例集成测试通过：blink、breathing_led、button、buzzer、led_chaser、playmusic（PIC10F200）。"
