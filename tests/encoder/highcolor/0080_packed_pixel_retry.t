#!/bin/sh
# Pixel allocation failure preserves packed input for retry.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0080_packed_pixel_retry || {
    echo "not ok 1 - high-color packed pixel retry"
    exit 0
}
echo "ok 1 - high-color packed pixel retry"
exit 0
