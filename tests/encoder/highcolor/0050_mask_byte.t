#!/bin/sh
# Byte RGB retains the separated alpha-zero mask.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0050_mask_byte || {
    echo "not ok 1 - high-color mask byte"
    exit 0
}
echo "ok 1 - high-color mask byte"
exit 0
