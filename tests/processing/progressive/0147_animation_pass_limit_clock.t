#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify animation pass limit clock.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0147_animation_pass_limit_clock" || {
    echo "not ok 1 - animation pass limit clock"
    exit 0
}
echo "ok 1 - animation pass limit clock"
exit 0
