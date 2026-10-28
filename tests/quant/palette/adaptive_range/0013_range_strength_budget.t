#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range strength budget.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0013_range_strength_budget" || {
    echo "not ok 1 - 0013_range_strength_budget"
    exit 0
}
echo "ok 1 - 0013_range_strength_budget"
exit 0
