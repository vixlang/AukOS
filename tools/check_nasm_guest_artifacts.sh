#!/bin/sh
set -eu

object=$1
executable=$2
limit=1048576

for artifact in "$object" "$executable"; do
    bytes=$(wc -c <"$artifact")
    test "$bytes" -le "$limit" || {
        echo "guest NASM artifact exceeds $limit bytes: $artifact ($bytes)" >&2
        exit 1
    }
    readelf -h "$artifact" | grep -q 'Class:.*ELF64'
    readelf -h "$artifact" | grep -q 'Data:.*little endian'
    readelf -h "$artifact" | grep -q 'Machine:.*X86-64'
done

readelf -h "$object" | grep -q 'Type:.*REL'
for section in .text .rodata .symtab .strtab .rela.text; do
    readelf -SW "$object" | grep -q " $section "
done
readelf -Ws "$object" | grep -Eq 'GLOBAL +DEFAULT +[0-9]+ +main$'
readelf -Ws "$object" | grep -Eq 'GLOBAL +DEFAULT +UND +aukos_vix_write_str$'
readelf -Wr "$object" | grep -Eq 'R_X86_64_PC32 +[^ ]+ +\.rodata - 4$'
readelf -Wr "$object" | grep -Eq 'R_X86_64_PLT32 +[^ ]+ +aukos_vix_write_str - 4$'

readelf -h "$executable" | grep -q 'Type:.*EXEC'
if readelf -W -l "$executable" | grep -Eq ' INTERP | DYNAMIC | TLS '; then
    echo 'guest-linked executable has dynamic or TLS program headers' >&2
    exit 1
fi
if readelf -W -l "$executable" | grep -Eq 'LOAD .*W.*E|LOAD .*E.*W'; then
    echo 'guest-linked executable has an RWX segment' >&2
    exit 1
fi

printf 'NASM_GUEST_AUDIT object=%s executable=%s elf64-rel-link-contract=yes\n' \
    "$(wc -c <"$object")" "$(wc -c <"$executable")"
