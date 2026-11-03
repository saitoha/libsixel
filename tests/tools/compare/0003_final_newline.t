#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
 "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/newline.txt" "$TOP_SRCDIR/tests/tools/compare/data/no-newline.txt" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - a missing final newline is detected'
    exit 0
}
echo 'ok 1 - a missing final newline is detected'
exit 0
