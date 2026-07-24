#!/bin/sh
set -eu

object=$1
limit=1048576

test "$(wc -c <"$object")" -le "$limit"
readelf -h "$object" | grep -q 'Class:.*ELF64'
readelf -h "$object" | grep -q 'Data:.*little endian'
readelf -h "$object" | grep -q 'Type:.*REL'
readelf -h "$object" | grep -q 'Machine:.*X86-64'

for section in .text .rodata .symtab .strtab .rela.text; do
    readelf -SW "$object" | grep -q " $section " || {
        echo "NASM reference object lacks $section" >&2
        exit 1
    }
done
readelf -Ws "$object" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +main$'
readelf -Ws "$object" | grep -Eq 'GLOBAL +DEFAULT +UND +aukos_vix_write_str$'
readelf -Wr "$object" | grep -Eq 'R_X86_64_PC32 +[^ ]+ +\.rodata - 4$'
readelf -Wr "$object" | grep -Eq 'R_X86_64_PLT32 +[^ ]+ +aukos_vix_write_str - 4$'

printf 'NASM_REFERENCE_AUDIT file=%s templates-contract=elf64-main-runtime\n' \
    "$(wc -c <"$object")"
