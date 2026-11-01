#!/bin/sh
# Crop retains the normalized fractional colors and mask.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0077_packed_crop || {
    echo "not ok 1 - high-color packed crop"
    exit 0
}
echo "ok 1 - high-color packed crop"
exit 0
