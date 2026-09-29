#!/bin/sh
# Verify wire exact cielab srgb.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 3 0 0 exact || {
    echo "not ok 1 - wire exact cielab srgb"
    exit 0
}
echo "ok 1 - wire exact cielab srgb"
exit 0
