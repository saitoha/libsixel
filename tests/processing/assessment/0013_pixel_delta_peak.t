#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify sparse pixel color errors independently of averaged quality.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0013_pixel_delta_peak max || {
    echo "not ok 1 - pixel_delta_peak"
    exit 0
}
echo "ok 1 - pixel_delta_peak"
exit 0
