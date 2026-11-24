#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# A fully occupied axis retains every nominal bin exactly.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0006_range_full_grid" || {
    printf '\n%s\n' "not ok 1 - 0006_range_full_grid"
    exit 0
}
printf '\n%s\n' "ok 1 - 0006_range_full_grid"
exit 0
