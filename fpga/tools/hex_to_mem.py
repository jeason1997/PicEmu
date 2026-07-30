#!/usr/bin/env python3
"""把 Microchip XC8 的 Intel HEX 转换为 256×12 位 readmemh 文件。"""

import argparse
from pathlib import Path
from typing import Dict, List


def load_hex(path: Path) -> List[int]:
    """读取 Intel HEX；PIC 程序字按小端序存放在两个连续字节中。"""
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
        data = record[4 : 4 + length]

        if (sum(record) & 0xFF) != 0:
            raise ValueError(f"line {line_number}: checksum mismatch")

        if record_type == 0x00:
            absolute = upper_address + address
            for offset, value in enumerate(data):
                byte_memory[absolute + offset] = value
        elif record_type == 0x01:
            break
        elif record_type == 0x04:
            upper_address = int.from_bytes(data, "big") << 16

    words = [0x000] * 256
    for word_address in range(256):
        byte_address = word_address * 2
        low = byte_memory.get(byte_address, 0)
        high = byte_memory.get(byte_address + 1, 0)
        words[word_address] = ((high << 8) | low) & 0x0FFF
    return words


def instruction_supported(instruction: int) -> bool:
    """当前第一阶段核心能够执行的 Baseline 指令集合。"""
    return (
        instruction == 0x000
        or ((instruction & 0xFF8) == 0x000 and (instruction & 0x7) == 6)
        or (instruction & 0xFE0) == 0x020
        or (instruction & 0xFE0) == 0x060
        or (instruction & 0xFC0) == 0x2C0
        or (instruction & 0xE00) == 0xA00
        or (instruction & 0xF00) == 0xC00
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="XC8 Intel HEX firmware")
    parser.add_argument("output", type=Path, help="12-bit readmemh output")
    args = parser.parse_args()

    words = load_hex(args.input)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if any(words[64:]):
        raise ValueError(
            "firmware exceeds the current Tang Nano 1K 64-word ROM stage"
        )
    unsupported = [
        (address, word)
        for address, word in enumerate(words[:64])
        if not instruction_supported(word)
    ]
    if unsupported:
        details = ", ".join(
            "0x{:02x}=0x{:03x}".format(address, word)
            for address, word in unsupported
        )
        raise ValueError("firmware uses unsupported instructions: " + details)
    args.output.write_text(
        "".join("{:03x}\n".format(word) for word in words[:64])
    )


if __name__ == "__main__":
    main()
