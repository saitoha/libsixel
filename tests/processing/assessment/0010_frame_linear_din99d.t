#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify frame linear din99d.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0010_frame_linear_din99d" || {
    echo "not ok 1 - frame linear din99d"
    exit 0
}
echo "ok 1 - frame linear din99d"
exit 0
