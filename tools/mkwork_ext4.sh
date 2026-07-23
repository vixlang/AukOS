#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 OUTPUT" >&2
    exit 2
fi

for tool in mke2fs truncate; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "error: required host tool not found: $tool" >&2
        exit 1
    }
done

output=$1
truncate -s 33554432 "$output"
E2FSPROGS_FAKE_TIME=1700000000 mke2fs -q -F -t ext4 -b 4096 -I 256 \
    -N 512 -g 8192 -m 0 -U 41554b4f-532d-574f-524b-4449534b3031 \
    -L AUKOS_WORK -O none,extent,filetype \
    -E lazy_itable_init=0,lazy_journal_init=0,root_owner=0:0 \
    "$output" 8184
