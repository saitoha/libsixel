#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range candidate budget.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0022_range_candidate_budget" || {
    printf '\n%s\n' "not ok 1 - range candidate budget"
    exit 0
}
printf '\n%s\n' "ok 1 - range candidate budget"
exit 0
