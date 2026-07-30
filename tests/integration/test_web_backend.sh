#!/bin/sh
set -eu

BACKEND=${1:-./build/picemu-web-core}
FIRMWARE=${2:-examples/button/build/firmware.hex}
SPI_FIRMWARE=${3:-examples/spi_flash/build/firmware.hex}

OUTPUT=$(
    printf 'load\t%s\tPIC10F200\nbreakpoints\t11\nrun\t10000\t8\t8\nrun\t10000\t8\t8\nw25_config\t2097152\t50 49 43 45 4D 55 21\t0\t1\t2\t3\nw25_read\t0\t16\nload\t%s\tPIC10F200\nw25_config\t2097152\t50 49 43 45 4D 55 21\t0\t1\t2\t3\nrun\t20000\t0\t0\nw25_read\t0\t16\n' "$FIRMWARE" "$SPI_FIRMWARE" |
        "$BACKEND"
)

printf '%s\n' "$OUTPUT" | python3 -c '
import json
import sys

states = [json.loads(line) for line in sys.stdin if line.strip()]
assert len(states) == 10, states
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
assert states[6]["ok"] and states[6]["device"] == "PIC10F200", states[6]
assert states[8]["cycles"] >= 20000, states[8]
assert states[8]["gpio"] & 0x04, states[8]
assert states[9]["data"][:7] == [0x50, 0x49, 0x43, 0x45, 0x4D, 0x55, 0x21], states[9]
print("Web backend protocol test passed.")
'
