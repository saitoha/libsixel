#!/bin/sh
# Verify edge soft gamma srgb grid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    edge 0 2 1 || {
    echo "not ok 1 - edge soft gamma srgb grid"
    exit 0
}
echo "ok 1 - edge soft gamma srgb grid"
exit 0
