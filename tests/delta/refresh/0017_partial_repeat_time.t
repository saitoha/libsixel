#!/bin/sh
# Verify partial repeat time.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0017_partial_repeat_time || {
    echo "not ok 1 - partial repeat time"
    exit 0
}
echo "ok 1 - partial repeat time"
exit 0
