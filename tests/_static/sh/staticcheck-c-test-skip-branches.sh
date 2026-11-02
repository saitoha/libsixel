#!/bin/sh
# Compile-time skips must not leave an unconditional, unreachable test body.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.c' --exclude-path '*/data/*' --exclude-path '*/_artifacts/*' \
    --exec awk -f "$src_root/tests/_static/awk/c-test-skip-branches.awk" >&2 || {
    echo 'not ok 1 - compile-time skips exclude the test body'
    exit 0
}
echo 'ok 1 - compile-time skips exclude the test body'
exit 0
