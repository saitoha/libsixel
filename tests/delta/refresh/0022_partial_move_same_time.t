#!/bin/sh
# Verify partial move same time.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0022_partial_move_same_time || {
    echo "not ok 1 - partial move same time"
    exit 0
}
echo "ok 1 - partial move same time"
exit 0
