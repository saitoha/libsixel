#!/bin/sh
# CLI string copies use bounded, portable CRT operations.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" \
    "$src_root/assessment" "$src_root/converters" \
    --name '*.c' --name '*.h' \
    --exec awk -f "$src_root/tests/_static/awk/cli-string-copy.awk" >&2 || {
    echo 'not ok 1 - CLI string copies use portable bounded operations'
    exit 0
}
echo 'ok 1 - CLI string copies use portable bounded operations'
exit 0
