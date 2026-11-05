#!/bin/sh
# Assessment code is also compiled outside the LSQA executable.
# Policy: docs/testing/staticcheck.md
# Policy: docs/build/test-harness.md
# Policy: docs/quality/color-evaluation.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/assessment" \
    --name '*.c' --name '*.h' \
    --exec awk -f "$src_root/tests/_static/awk/assessment-fopen.awk" >&2 || {
    echo 'not ok 1 - assessment consumers use portable file paths'
    exit 0
}
echo 'ok 1 - assessment consumers use portable file paths'
exit 0
