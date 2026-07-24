#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${1:-"$root/build/vixc-core-host"}
tmp=$(mktemp -d /tmp/vix-elf-linker.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

"$compiler" -obj "$root/tests/elf_linker_app.vix" -o "$tmp/app.o"
clang -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mcmodel=large \
    -c "$root/tests/elf_linker_host_runtime.c" -o "$tmp/runtime.o"
clang -std=c17 -Wall -Wextra -Werror "$root/tests/elf_linker_test.c" \
    -o "$tmp/linker-test"
"$tmp/linker-test" "$tmp/app.o" "$tmp/runtime.o" "$tmp/app.elf"

readelf -h -S -l "$tmp/app.elf" >"$tmp/readelf.txt"
objdump -d -Mintel "$tmp/app.elf" >"$tmp/objdump.txt"
grep -q 'Type:.*EXEC' "$tmp/readelf.txt"
grep -q '\.text' "$tmp/readelf.txt"
grep -q '\.rodata' "$tmp/readelf.txt"
grep -q '\.data' "$tmp/readelf.txt"
grep -q '\.bss' "$tmp/readelf.txt"
test "$(grep -c 'LOAD' "$tmp/readelf.txt")" -eq 3
! grep -Eq 'INTERP|DYNAMIC|TLS' "$tmp/readelf.txt"

chmod +x "$tmp/app.elf"
set +e
"$tmp/app.elf"
code=$?
set -e
test "$code" -eq 42

echo 'elf_linker_integration: PASS'
