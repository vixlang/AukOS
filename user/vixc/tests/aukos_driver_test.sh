#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${1:-"$root/build/vixc-core-host"}
compiler=$(CDPATH= cd -- "$(dirname -- "$compiler")" && pwd)/$(basename -- "$compiler")
tmp=$(mktemp -d /tmp/vix-driver-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat >"$tmp/program.vix" <<'SOURCE'
fn main(): i32 { return 42 }
SOURCE
cat >"$tmp/bad.vix" <<'SOURCE'
fn broken( {
SOURCE

clang -std=c17 -ffreestanding -fno-stack-protector -fno-pic -mcmodel=large \
    -c "$root/tests/elf_linker_host_runtime.c" -o "$tmp/runtime.o"
runtime=$tmp/runtime.o

"$compiler" --help >"$tmp/help"
"$compiler" --version >"$tmp/version"
grep -Fx 'vixc 0.4.6' "$tmp/version"
grep -F 'vixc INPUT.vix [-o OUTPUT]' "$tmp/help"
grep -F 'By default, compile INPUT.vix and statically link an AukOS executable.' "$tmp/help"
grep -F -- '--target TRIPLE' "$tmp/help"
grep -F -- '--link' "$tmp/help"
if grep -E 'LLVM|--backend|-ll|-opt=' "$tmp/help"; then
    echo 'AukOS help advertises an unsupported host option' >&2
    exit 1
fi

expect_failure() {
    name=$1
    shift
    if "$compiler" "$@" >"$tmp/$name.out" 2>"$tmp/$name.err"; then
        echo "$name unexpectedly succeeded" >&2
        exit 1
    fi
    test -s "$tmp/$name.err"
}

expect_failure unknown --unknown
expect_failure missing-output -o
expect_failure missing-target --target
expect_failure multiple-input "$tmp/program.vix" "$tmp/program.vix"
expect_failure conflicting-mode -obj --link "$tmp/program.vix"

cp "$tmp/program.vix" "$tmp/hello.vix"
cp "$tmp/program.vix" "$tmp/tool"
cp "$tmp/program.vix" "$tmp/a.b.vix"
cp "$tmp/program.vix" "$tmp/-source.vix"
(
    cd "$tmp"
    VIXC_RUNTIME_OBJECT="$runtime" "$compiler" hello.vix
    VIXC_RUNTIME_OBJECT="$runtime" "$compiler" tool
    VIXC_RUNTIME_OBJECT="$runtime" "$compiler" a.b.vix
    VIXC_RUNTIME_OBJECT="$runtime" "$compiler" program.vix -o exact-output
    "$compiler" -obj -o dash.o -- -source.vix
    test -f hello
    test -f tool.elf
    test -f a.b
    test -f exact-output
    test -f dash.o
    test -x hello
    test -x tool.elf
    test -x a.b
    test -x exact-output
)

(
    cd "$tmp"
    "$compiler" -obj program.vix
    test -f program.o
    VIXC_RUNTIME_OBJECT="$runtime" "$compiler" --link program.o
    test -f program
    test -x program
)

before=$(find /tmp -maxdepth 1 -type f -name 'vixc-object-*' -print | sort)
expect_failure compile-failure "$tmp/bad.vix" -o "$tmp/compile-failure-output"
test ! -e "$tmp/compile-failure-output"
printf 'preserve-me\n' >"$tmp/existing-output"
if VIXC_RUNTIME_OBJECT="$tmp/missing-runtime.o" \
    "$compiler" "$tmp/program.vix" -o "$tmp/existing-output" \
    >"$tmp/link-failure.out" 2>"$tmp/link-failure.err"; then
    echo 'link failure vector unexpectedly succeeded' >&2
    exit 1
fi
grep -F 'link stage' "$tmp/link-failure.err"
test "$(cat "$tmp/existing-output")" = preserve-me
after=$(find /tmp -maxdepth 1 -type f -name 'vixc-object-*' -print | sort)
test "$before" = "$after"

VIXC_RUNTIME_OBJECT="$runtime" "$compiler" "$tmp/program.vix" -o "$tmp/parallel-a" &
first_pid=$!
VIXC_RUNTIME_OBJECT="$runtime" "$compiler" "$tmp/program.vix" -o "$tmp/parallel-b" &
second_pid=$!
wait "$first_pid"
wait "$second_pid"
test -f "$tmp/parallel-a"
test -f "$tmp/parallel-b"
final=$(find /tmp -maxdepth 1 -type f -name 'vixc-object-*' -print | sort)
test "$before" = "$final"

echo 'aukos_driver_test: PASS'
