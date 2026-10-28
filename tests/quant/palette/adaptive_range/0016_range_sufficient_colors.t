#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range sufficient colors.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0016_range_sufficient_colors" || {
    echo "not ok 1 - 0016_range_sufficient_colors"
    exit 0
}
echo "ok 1 - 0016_range_sufficient_colors"
exit 0
