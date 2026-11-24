#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Retain dark ramp steps and sparse bright UI colors.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0003_range_medoids" || {
    printf '\n%s\n' "not ok 1 - 0003_range_medoids"
    exit 0
}
printf '\n%s\n' "ok 1 - 0003_range_medoids"
exit 0

