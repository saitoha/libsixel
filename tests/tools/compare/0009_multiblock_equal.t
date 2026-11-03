#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
"$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/images/snake.png" "$TOP_SRCDIR/images/snake.png" || {
    echo 'not ok 1 - files larger than the comparison buffer compare equal'
    exit 0
}
echo 'ok 1 - files larger than the comparison buffer compare equal'
exit 0
