#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify frame linear oklab.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0009_frame_linear_oklab" || {
    printf '\n%s\n' "not ok 1 - frame linear oklab"
    exit 0
}
printf '\n%s\n' "ok 1 - frame linear oklab"
exit 0
