#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify animation tail time limit.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0148_animation_tail_time_limit" || {
    echo "not ok 1 - animation tail time limit"
    exit 0
}
echo "ok 1 - animation tail time limit"
exit 0
