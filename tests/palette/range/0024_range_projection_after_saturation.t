#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range projection after saturation.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0024_range_projection_after_saturation" || {
    printf '\n%s\n' "not ok 1 - range projection after saturation"
    exit 0
}
printf '\n%s\n' "ok 1 - range projection after saturation"
exit 0
