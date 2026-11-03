#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
 "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data" "$TOP_SRCDIR/tests/tools/compare/data/empty" || status=$?
test "$status" -eq 2 || {
    echo 'not ok 1 - a directory input is a read error'
    exit 0
}
echo 'ok 1 - a directory input is a read error'
exit 0
