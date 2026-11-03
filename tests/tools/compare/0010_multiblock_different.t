#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
{ cat "$TOP_SRCDIR/images/snake.png"; printf x; } | \
    "$SIXEL_TEST_COMPARE" -s - "$TOP_SRCDIR/images/snake.png" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - a difference after several buffers is detected'
    exit 0
}
echo 'ok 1 - a difference after several buffers is detected'
exit 0
