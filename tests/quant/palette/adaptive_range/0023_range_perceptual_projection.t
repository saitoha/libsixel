#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range perceptual projection.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0023_range_perceptual_projection" || {
    echo "not ok 1 - range perceptual projection"
    exit 0
}
echo "ok 1 - range perceptual projection"
exit 0
