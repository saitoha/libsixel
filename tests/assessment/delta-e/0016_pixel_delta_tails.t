#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify sparse pixel color errors independently of averaged quality.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0013_pixel_delta_peak tails || {
    printf '\n%s\n' "not ok 1 - pixel_delta_tails"
    exit 0
}
printf '\n%s\n' "ok 1 - pixel_delta_tails"
exit 0
