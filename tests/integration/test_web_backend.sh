#!/bin/sh
set -eu

BACKEND=${1:-./build/picemu-web-core}
FIRMWARE=${2:-examples/button/build/firmware.hex}
SPI_FIRMWARE=${3:-examples/spi_flash/build/firmware.hex}
LCD_FIRMWARE=${4:-examples/i2c_lcd1602/build/firmware.hex}
SEVEN_SEGMENT_FIRMWARE=${5:-examples/seven_segment/build/firmware.hex}

OUTPUT=$(
    printf 'load\t%s\tPIC10F200\nbreakpoints\t11\nrun\t10000\t8\t8\nrun\t10000\t8\t8\nw25_config\t2097152\t50 49 43 45 4D 55 21\t0\t1\t2\t3\nw25_read\t0\t16\nw25_write\t0\tAA BB\nw25_read\t0\t4\nload\t%s\tPIC10F202\nw25_config\t2097152\t\t0\t1\t2\t3\nrun\t20000\t0\t0\nw25_read\t0\t16\nload\t%s\tPIC10F200\nlcd1602_config\t39\t0\t1\nrun\t200000\t0\t0\nload\t%s\tPIC10F200\nhc595_config\t0\t1\t2\nseven_segment_config\t1\nrun\t300000\t0\t0\n' "$FIRMWARE" "$SPI_FIRMWARE" "$LCD_FIRMWARE" "$SEVEN_SEGMENT_FIRMWARE" |
        "$BACKEND"
)

printf '%s\n' "$OUTPUT" | python3 -c '
import json
import sys

states = [json.loads(line) for line in sys.stdin if line.strip()]
assert len(states) == 19, states
assert states[0]["ok"] and states[0]["device"] == "PIC10F200", states[0]
assert len(states[0]["flash"]) == 256, len(states[0]["flash"])
assert len(states[0]["instructions"]) == 256, len(states[0]["instructions"])
assert states[0]["instructions"][0], states[0]["instructions"][0]
assert states[2]["breakpointHit"] and states[2]["pc"] == 11, states[2]
assert states[3]["breakpointHit"] and states[3]["pc"] == 11, states[3]
assert states[3]["cycles"] > states[2]["cycles"], states[3]
assert len(states[3]["ram"]) == 32, len(states[3]["ram"])
assert states[4]["ok"], states[4]
assert states[5]["capacity"] == 2097152, states[5]
assert states[5]["data"][:7] == [0x50, 0x49, 0x43, 0x45, 0x4D, 0x55, 0x21], states[5]
assert states[6]["ok"], states[6]
assert states[7]["data"] == [0xAA, 0xBB, 0x43, 0x45], states[7]
assert states[8]["ok"] and states[8]["device"] == "PIC10F202", states[8]
assert states[10]["cycles"] >= 20000, states[10]
assert states[10]["gpio"] & 0x04, states[10]
assert states[11]["offset"] == 0, states[11]
assert states[11]["data"][:10] == list(b"HelloWorld"), states[11]
assert states[12]["device"] == "PIC10F200", states[12]
assert states[13]["lcd1602"]["address"] == 0x27, states[13]
assert states[14]["lcd1602"]["lines"][0].startswith("PIC10F200"), states[14]
assert states[15]["device"] == "PIC10F200", states[15]
assert states[16]["hc595"]["outputs"] == 0, states[16]
assert states[17]["sevenSegment"]["segments"] == 0, states[17]
assert states[18]["sevenSegment"]["segments"] in (
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
), states[18]
print("Web backend protocol test passed.")
'
