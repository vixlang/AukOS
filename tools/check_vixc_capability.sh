#!/bin/sh
set -eu

compiler=$1
version=$($compiler --version)
help=$($compiler --help)

printf '%s\n' "$version" | grep -Eq '^vixc [0-9]+\.[0-9]+\.[0-9]+ \(bootstrap\)$'
printf '%s\n' "$help" | grep -q -- '--target <triple>'
printf '%s\n' "$help" | grep -q -- '-obj'
printf '%s\n' "$help" | grep -q -- '--backend <llvm|self|self-opt|self-lir>'

printf 'VIXC_GATE %s\n' "$version"
