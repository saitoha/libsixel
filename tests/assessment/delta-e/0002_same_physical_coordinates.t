#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify same physical coordinates.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0002_same_physical_coordinates" || {
    printf '\n%s\n' "not ok 1 - same physical coordinates"
    exit 0
}
printf '\n%s\n' "ok 1 - same physical coordinates"
exit 0
