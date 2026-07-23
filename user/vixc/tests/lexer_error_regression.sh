#!/bin/sh
set -eu

compiler=${1:?usage: $0 COMPILER}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/vixc-lexer-regression.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

invalid_source="$tmpdir/invalid.vix"
missing_object="$tmpdir/missing.o"
invalid_object="$tmpdir/invalid.o"
invalid_stderr="$tmpdir/invalid.stderr"
invalid_stdout="$tmpdir/invalid.stdout"
normal_object="$tmpdir/normal.o"

printf '/* unterminated block comment\n' > "$invalid_source"

run_invalid() {
    if "$compiler" --target x86_64-unknown-none -obj "$invalid_source" -o "$1" \
        >"$invalid_stdout" 2>"$invalid_stderr"; then
        echo 'FAIL: invalid lexer input unexpectedly compiled' >&2
        exit 1
    fi
    grep -Fq 'vixc: lexer error: unterminated block comment' "$invalid_stderr"
}

run_invalid "$missing_object"
test ! -e "$missing_object"

printf 'preserve this object\n' > "$invalid_object"
run_invalid "$invalid_object"
test "$(cat "$invalid_object")" = 'preserve this object'

"$compiler" --target x86_64-unknown-none -obj \
    "$(dirname "$0")/block_comment_regression.vix" -o "$normal_object"
test -s "$normal_object"
readelf -h "$normal_object" | grep -Eq 'Type:[[:space:]]+REL'

echo 'lexer error regression: PASS'
