#!/bin/sh
set -eu

elf=$1
limit=1048576
bytes=$(wc -c <"$elf")

test "$bytes" -le "$limit" || {
    echo "NASM file exceeds VFS limit: $bytes > $limit" >&2
    exit 1
}
readelf -h "$elf" | grep -q 'Class:.*ELF64'
readelf -h "$elf" | grep -q 'Type:.*EXEC'
readelf -h "$elf" | grep -q 'Machine:.*X86-64'
if readelf -W -l "$elf" | grep -Eq ' INTERP | DYNAMIC | TLS '; then
    echo 'NASM artifact contains a dynamic-loader or TLS program header' >&2
    exit 1
fi
if readelf -W -l "$elf" | grep -Eq 'LOAD .*W.*E|LOAD .*E.*W'; then
    echo 'NASM artifact contains a writable executable segment' >&2
    exit 1
fi
if readelf -W -l "$elf" | grep 'GNU_STACK' | grep -q 'E'; then
    echo 'NASM artifact requests an executable stack' >&2
    exit 1
fi
if test -n "$(nm -u "$elf")"; then
    echo 'NASM artifact has undefined symbols' >&2
    nm -u "$elf" >&2
    exit 1
fi
if strings "$elf" | grep -Eq 'GLIBC_|/lib(64)?/ld-|/usr/(lib|include)'; then
    echo 'NASM artifact contains a host runtime dependency' >&2
    exit 1
fi

printf 'NASM_AUDIT file=%s limit=%s static=yes tls=no rwx=no execstack=no undefined=0\n' \
    "$bytes" "$limit"
