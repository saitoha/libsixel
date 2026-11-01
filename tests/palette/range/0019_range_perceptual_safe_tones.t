#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range perceptual safe tones.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0019_range_perceptual_safe_tones" || {
    echo "not ok 1 - 0019_range_perceptual_safe_tones"
    exit 0
}
echo "ok 1 - 0019_range_perceptual_safe_tones"
exit 0
