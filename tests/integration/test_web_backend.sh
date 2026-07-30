#!/bin/sh
set -eu

BACKEND=${1:-./build/picemu-web-core}
FIRMWARE=${2:-examples/button/build/firmware.hex}

OUTPUT=$(
    printf 'load\t%s\tPIC10F200\nrun\t10000\t8\t8\n' "$FIRMWARE" |
        "$BACKEND"
)

printf '%s\n' "$OUTPUT" | python3 -c '
import json
import sys

states = [json.loads(line) for line in sys.stdin if line.strip()]
assert len(states) == 2, states
assert states[0]["ok"] and states[0]["device"] == "PIC10F200", states[0]
assert len(states[0]["flash"]) == 256, len(states[0]["flash"])
assert len(states[0]["instructions"]) == 256, len(states[0]["instructions"])
assert states[0]["instructions"][0], states[0]["instructions"][0]
assert states[1]["ok"] and states[1]["cycles"] >= 10000, states[1]
assert len(states[1]["ram"]) == 32, len(states[1]["ram"])
print("Web backend protocol test passed.")
'
