#!/bin/sh
# Assessment spools share the portable CRT stream contract with C tests.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
sh "$1/tests/_static/sh/list-source-files.sh" "$1/assessment" \
    --name '*.c' --name '*.h' \
    --exec awk -f "$1/tests/_static/awk/c-test-stdio.awk" >&2 || {
    echo 'not ok 1 - assessment spools use portable CRT streams'
    exit 0
}
echo 'ok 1 - assessment spools use portable CRT streams'
exit 0
