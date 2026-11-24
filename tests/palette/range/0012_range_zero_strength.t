#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range zero strength.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0012_range_zero_strength" || {
    printf '\n%s\n' "not ok 1 - 0012_range_zero_strength"
    exit 0
}
printf '\n%s\n' "ok 1 - 0012_range_zero_strength"
exit 0
