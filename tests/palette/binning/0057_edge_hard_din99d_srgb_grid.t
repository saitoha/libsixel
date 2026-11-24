#!/bin/sh
# Verify edge hard din99d srgb grid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    edge 4 1 1 || {
    printf '\n%s\n' "not ok 1 - edge hard din99d srgb grid"
    exit 0
}
printf '\n%s\n' "ok 1 - edge hard din99d srgb grid"
exit 0
