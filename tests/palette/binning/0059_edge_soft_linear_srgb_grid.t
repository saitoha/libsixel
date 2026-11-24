#!/bin/sh
# Verify edge soft linear srgb grid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    edge 1 2 1 || {
    printf '\n%s\n' "not ok 1 - edge soft linear srgb grid"
    exit 0
}
printf '\n%s\n' "ok 1 - edge soft linear srgb grid"
exit 0
