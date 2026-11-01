#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range candidate strength.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0025_range_candidate_strength" || {
    echo "not ok 1 - range candidate strength"
    exit 0
}
echo "ok 1 - range candidate strength"
exit 0
