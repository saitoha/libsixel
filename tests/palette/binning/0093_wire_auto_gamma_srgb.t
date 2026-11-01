#!/bin/sh
# Verify wire auto gamma srgb.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 0 0 0 auto || {
    echo "not ok 1 - wire auto gamma srgb"
    exit 0
}
echo "ok 1 - wire auto gamma srgb"
exit 0
