#!/usr/bin/env python3
"""Pack NASM's generated assembler templates without dropping instructions."""

import re
import sys
from pathlib import Path


TEMPLATE_RE = re.compile(
    r"^\s*\{I_([A-Z0-9_]+),\s*(\d+),\s*\{([^}]*)\},\s*"
    r"(NO_DECORATOR|\{[^}]*\}),\s*nasm_bytecodes\+(\d+),\s*"
    r"(\d+),\s*(\d+)\},\s*$"
)
LIST_RE = re.compile(r"^\s*\{\s*(\d+),\s*instrux_([A-Z0-9_]+)\s*\},\s*$")
EMPTY_RE = re.compile(r"^\s*\{\s*0,\s*NULL\s*/\*\s*([A-Z0-9_]+)\s*\*/\s*\},\s*$")


def intern(values, table, indices):
    key = ",".join(part.strip() for part in values.split(","))
    if key not in indices:
        indices[key] = len(table)
        table.append(key)
    return indices[key]


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: compact_insns.py INPUT-insnsa.c OUTPUT.c")

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    templates = {}
    operands = []
    operand_indices = {}
    decorators = []
    decorator_indices = {}
    opcode_order = []
    in_list = False

    for line_number, line in enumerate(input_path.read_text().splitlines(), 1):
        match = TEMPLATE_RE.match(line)
        if match:
            opcode, count, ops, decos, code, iflags, source_line = match.groups()
            if decos == "NO_DECORATOR":
                decos = "0,0,0,0,0"
            else:
                decos = decos[1:-1]
            templates.setdefault(opcode, []).append(
                (
                    int(code),
                    intern(ops, operands, operand_indices),
                    intern(decos, decorators, decorator_indices),
                    int(iflags),
                    int(source_line),
                    int(count),
                )
            )
            continue

        if line.startswith("const struct itemplate_list nasm_instructions"):
            in_list = True
            continue
        if in_list:
            match = LIST_RE.match(line) or EMPTY_RE.match(line)
            if match:
                count, opcode = match.groups() if len(match.groups()) == 2 else ("0", match.group(1))
                actual = len(templates.get(opcode, []))
                if actual != int(count):
                    raise SystemExit(
                        f"{input_path}:{line_number}: {opcode} has {actual} templates, expected {count}"
                    )
                opcode_order.append(opcode)
            elif line.strip() == "};":
                in_list = False

    template_count = sum(len(group) for group in templates.values())
    if template_count < 10000 or len(opcode_order) < 1000:
        raise SystemExit("refusing incomplete NASM instruction table")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w") as output:
        output.write("/* Generated from official NASM insnsa.c; do not edit. */\n")
        output.write('#include "nasm.h"\n#include "insns.h"\n\n')
        output.write("const opflags_t nasm_compact_operands[][MAX_OPERANDS] = {\n")
        for value in operands:
            output.write(f"    {{{value}}},\n")
        output.write("};\n\n")
        output.write("const decoflags_t nasm_compact_decorators[][MAX_OPERANDS] = {\n")
        for value in decorators:
            output.write(f"    {{{value}}},\n")
        output.write("};\n\n")
        output.write("const struct compact_itemplate nasm_compact_templates[] = {\n")
        for opcode in opcode_order:
            for code, ops, decos, iflags, source_line, count in templates.get(opcode, []):
                output.write(
                    f"    {{{code}, {ops}, {decos}, {iflags}, {source_line}, {count}}},\n"
                )
        output.write("};\n\n")
        output.write("const struct itemplate_list nasm_instructions[] = {\n")
        first = 0
        for opcode in opcode_order:
            count = len(templates.get(opcode, []))
            output.write(f"    {{{first}, {count}}}, /* I_{opcode} */\n")
            first += count
        output.write("};\n")

    print(
        f"packed {template_count} templates, {len(operands)} operand sets, "
        f"{len(decorators)} decorator sets",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()

