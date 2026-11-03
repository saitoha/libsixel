#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
 "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/empty" "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - an empty file differs from nonempty data'
    exit 0
}
echo 'ok 1 - an empty file differs from nonempty data'
exit 0
