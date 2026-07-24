#!/usr/bin/env python3
"""Verify the compact instruction table is lossless against NASM's generator."""

import re
import sys
from pathlib import Path


SOURCE_TEMPLATE = re.compile(
    r"^\s*\{I_([A-Z0-9_]+),\s*(\d+),\s*\{([^}]*)\},\s*"
    r"(NO_DECORATOR|\{[^}]*\}),\s*nasm_bytecodes\+(\d+),\s*(\d+),\s*(\d+)\},\s*$"
)
COMPACT_TEMPLATE = re.compile(
    r"^\s*\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\},\s*$"
)


def normalized(values):
    return tuple(part.strip() for part in values.split(","))


def parse_source(path):
    records = []
    for line in path.read_text().splitlines():
        match = SOURCE_TEMPLATE.match(line)
        if not match:
            continue
        opcode, count, operands, decorators, code, flags, source_line = match.groups()
        if decorators == "NO_DECORATOR":
            decorators = "0,0,0,0,0"
        else:
            decorators = decorators[1:-1]
        records.append(
            (opcode, int(code), normalized(operands), normalized(decorators),
             int(flags), int(source_line), int(count))
        )
    return records


def parse_array(lines, declaration):
    values = []
    active = False
    for line in lines:
        if line.startswith(declaration):
            active = True
            continue
        if active and line.strip() == "};":
            return values
        if active:
            values.append(normalized(line.strip()[1:-2]))
    raise AssertionError(f"missing array {declaration}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_compact_insns.py ORIGINAL COMPACT")
    source = parse_source(Path(sys.argv[1]))
    lines = Path(sys.argv[2]).read_text().splitlines()
    operands = parse_array(lines, "const opflags_t nasm_compact_operands")
    decorators = parse_array(lines, "const decoflags_t nasm_compact_decorators")
    compact = []
    active = False
    for line in lines:
        if line.startswith("const struct compact_itemplate nasm_compact_templates"):
            active = True
            continue
        if active and line.strip() == "};":
            break
        if active:
            match = COMPACT_TEMPLATE.match(line)
            if not match:
                raise AssertionError(f"unparsed compact template: {line}")
            code, ops, decos, flags, source_line, count = map(int, match.groups())
            compact.append((code, operands[ops], decorators[decos], flags, source_line, count))

    assert len(source) == len(compact) >= 10000
    for original, packed in zip(source, compact):
        assert original[1:] == packed, (original, packed)
    print(f"NASM_COMPACT_AUDIT templates={len(compact)} lossless=yes")


if __name__ == "__main__":
    main()
