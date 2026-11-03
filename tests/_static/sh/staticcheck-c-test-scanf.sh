#!/bin/sh
# Numeric parsing in shared C runner tests must use the CRT compatibility API.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.c' --name '*.h' \
    --exclude-path '*/data/*' --exclude-path '*/extension/*' \
    --exec awk -f "$src_root/tests/_static/awk/c-test-scanf.awk" >&2 || {
    echo 'not ok 1 - shared C tests use portable CRT scanning'
    exit 0
}
echo 'ok 1 - shared C tests use portable CRT scanning'
exit 0
