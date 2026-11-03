#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
 "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/empty" "$TOP_SRCDIR/tests/tools/compare/data/empty" || status=$?
test "$status" -eq 0 || {
    echo 'not ok 1 - two empty files compare equal'
    exit 0
}
echo 'ok 1 - two empty files compare equal'
exit 0
