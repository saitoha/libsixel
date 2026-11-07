#!/bin/sh
# Shared C runner tests avoid deprecated CRT operations.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.c' --name '*.h' \
    --exclude-path '*/data/*' --exclude-path '*/extension/*' \
    --exec awk -f "$src_root/tests/_static/awk/c-test-crt.awk" >&2 || {
    echo 'not ok 1 - shared C tests use portable CRT operations'
    exit 0
}
echo 'ok 1 - shared C tests use portable CRT operations'
exit 0
