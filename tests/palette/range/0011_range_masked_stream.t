#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Preserve native sample coordinates and visible population during binning.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0011_range_masked_stream" || {
    printf '\n%s\n' "not ok 1 - 0011_range_masked_stream"
    exit 0
}
printf '\n%s\n' "ok 1 - 0011_range_masked_stream"
exit 0
