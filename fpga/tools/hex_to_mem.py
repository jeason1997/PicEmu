#!/usr/bin/env python3
"""把 Microchip XC8 Intel HEX 转换为 PIC10F200 的 256×12 位 ROM 镜像。"""

import argparse
from pathlib import Path
from typing import Dict, List


def load_hex(path: Path) -> List[int]:
    """PIC 程序字以小端序存放在两个连续的 Intel HEX 字节中。"""
    byte_memory: Dict[int, int] = {}
    upper_address = 0
    for line_number, raw_line in enumerate(path.read_text().splitlines(), 1):
        line = raw_line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"line {line_number}: missing ':'")
        record = bytes.fromhex(line[1:])
        length = record[0]
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        data = record[4:4 + length]
        if sum(record) & 0xFF:
            raise ValueError(f"line {line_number}: checksum mismatch")
        if record_type == 0x00:
            absolute = upper_address + address
            for offset, value in enumerate(data):
                byte_memory[absolute + offset] = value
        elif record_type == 0x01:
            break
        elif record_type == 0x04:
            upper_address = int.from_bytes(data, "big") << 16

    words = [0] * 256
    for word_address in range(256):
        byte_address = word_address * 2
        low = byte_memory.get(byte_address, 0)
        high = byte_memory.get(byte_address + 1, 0)
        words[word_address] = ((high << 8) | low) & 0x0FFF
    return words


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="XC8 Intel HEX firmware")
    parser.add_argument("output", type=Path, help="256×12-bit readmemh output")
    args = parser.parse_args()
    words = load_hex(args.input)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "".join(f"{word:03x}\n" for word in words), encoding="ascii"
    )


if __name__ == "__main__":
    main()
