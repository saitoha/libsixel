#!/bin/sh
# Mask allocation failure preserves packed input for retry.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0081_packed_mask_retry || {
    echo "not ok 1 - high-color packed mask retry"
    exit 0
}
echo "ok 1 - high-color packed mask retry"
exit 0
