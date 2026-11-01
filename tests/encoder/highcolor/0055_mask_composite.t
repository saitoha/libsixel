#!/bin/sh
# A retained composite-fallback mask requests P2=1.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0055_mask_composite || {
    echo "not ok 1 - high-color mask composite"
    exit 0
}
echo "ok 1 - high-color mask composite"
exit 0
