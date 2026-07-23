#!/bin/sh
set -eu

elf=$1
core=$2
limit=1048576

readelf -h "$core" | grep -q 'Type:.*REL'
readelf -h "$elf" | grep -q 'Type:.*EXEC'
readelf -h "$elf" | grep -q 'Machine:.*X86-64'

if readelf -W -l "$elf" | grep -Eq 'INTERP|DYNAMIC|TLS'; then
    echo 'vixc artifact contains a dynamic-loader or TLS program header' >&2
    exit 1
fi
if readelf -W -l "$elf" | grep -Eq 'LOAD.*W.*E|LOAD.*E.*W'; then
    echo 'vixc artifact contains a writable executable segment' >&2
    exit 1
fi
if test -n "$(nm -u "$elf")"; then
    echo 'vixc artifact has undefined symbols' >&2
    nm -u "$elf" >&2
    exit 1
fi
if nm -a "$elf" | grep -Eiq 'LLVM|lld|pthread|GLIBC|GLIBCXX|CXXABI|(^|[[:space:]])_Z'; then
    echo 'vixc artifact contains a forbidden host dependency symbol' >&2
    exit 1
fi

set -- $(size "$elf" | awk 'NR == 2 { print $1, $2, $3 }')
for bytes in "$@"; do
    test "$bytes" -le "$limit" || {
        echo "vixc text/data/BSS component exceeds $limit bytes" >&2
        exit 1
    }
done
bytes=$(wc -c <"$elf")
test "$bytes" -le "$limit" || {
    echo "vixc file exceeds tmpfs limit: $bytes > $limit" >&2
    exit 1
}

printf 'VIXC_AUDIT file=%s text=%s data=%s bss=%s\n' "$bytes" "$1" "$2" "$3"
