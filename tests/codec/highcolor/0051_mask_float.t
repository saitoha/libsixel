#!/bin/sh
# Float RGB retains the separated alpha-zero mask.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0051_mask_float || {
    echo "not ok 1 - high-color mask float"
    exit 0
}
echo "ok 1 - high-color mask float"
exit 0
