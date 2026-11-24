#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify the resolved model controls the candidate budget.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0027_range_model_budget" || {
    printf '\n%s\n' "not ok 1 - resolved model candidate budget"
    exit 0
}
printf '\n%s\n' "ok 1 - resolved model candidate budget"
exit 0
