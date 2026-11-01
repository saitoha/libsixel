#!/bin/sh
# A high-color source mask requests P2=1 for keep.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0053_mask_keep || {
    echo "not ok 1 - high-color mask keep"
    exit 0
}
echo "ok 1 - high-color mask keep"
exit 0
