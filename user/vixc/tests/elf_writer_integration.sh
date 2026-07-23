#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${1:-"$root/build/vixc-core-host"}
tmp=$(mktemp -d /tmp/vix-elf-integration.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

check_object() {
    source=$1
    name=$2
    object="$tmp/$name.o"
    executable="$tmp/$name"

    "$compiler" -obj "$source" -o "$object"
    readelf -h -S -s -r "$object" >"$tmp/$name.readelf"
    objdump -dr -Mintel "$object" >"$tmp/$name.objdump"

    grep -q 'Type:.*REL' "$tmp/$name.readelf"
    grep -q '\.text' "$tmp/$name.readelf"
    grep -q '\.rodata' "$tmp/$name.readelf"
    grep -q '\.rela.text' "$tmp/$name.readelf"
    grep -q '\.symtab' "$tmp/$name.readelf"
    grep -q 'R_X86_64_PLT32' "$tmp/$name.readelf"
    grep -q 'R_X86_64_PC32' "$tmp/$name.readelf"

    clang -fuse-ld=lld -no-pie -o "$executable" "$object" \
        "$root/runtime/runtime.o"
    "$executable"
}

check_object "$root/tests/elf_writer_integration.vix" integration
check_object "$root/tests/files/test20.vix" float
check_object "$root/tests/files/test21.vix" float_abi

if "$compiler" --lex "$root/tests/lexer_error_regression.vix" \
    >"$tmp/lexer.out" 2>"$tmp/lexer.err"; then
    echo 'unterminated string unexpectedly passed --lex' >&2
    exit 1
fi
grep -q 'lexer error: unterminated string literal' "$tmp/lexer.err"

echo 'elf_writer_integration: PASS'
