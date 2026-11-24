#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# All dark steps remain distinct and sparse highlights keep separate bins.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0005_range_refinement" || {
    printf '\n%s\n' "not ok 1 - 0005_range_refinement"
    exit 0
}
printf '\n%s\n' "ok 1 - 0005_range_refinement"
exit 0
