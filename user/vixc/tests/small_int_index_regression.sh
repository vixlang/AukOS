#!/bin/sh
set -eu

compiler=${1:?usage: $0 COMPILER}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/vixc-small-int-regression.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

program="$tmpdir/small-int-index"
"$compiler" "$(dirname "$0")/small_int_index_regression.vix" -o "$program"
"$program"

echo 'small integer index regression: PASS'
