#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify frame linear lab.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0008_frame_linear_lab" || {
    echo "not ok 1 - frame linear lab"
    exit 0
}
echo "ok 1 - frame linear lab"
exit 0
