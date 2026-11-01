#!/bin/sh
# Verify partial move x.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0014_partial_move_x || {
    echo "not ok 1 - partial move x"
    exit 0
}
echo "ok 1 - partial move x"
exit 0
