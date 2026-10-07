#!/bin/sh
# An all-zero mask does not omit visible pixels.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0063_mask_zero || {
    echo "not ok 1 - high-color mask zero"
    exit 0
}
echo "ok 1 - high-color mask zero"
exit 0
