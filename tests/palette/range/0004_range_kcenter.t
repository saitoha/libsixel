#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Retain dark ramp steps and sparse bright UI colors.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0004_range_kcenter" || {
    echo "not ok 1 - 0004_range_kcenter"
    exit 0
}
echo "ok 1 - 0004_range_kcenter"
exit 0

