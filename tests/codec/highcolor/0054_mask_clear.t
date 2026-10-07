#!/bin/sh
# A high-color source mask explicitly requests P2=0 for clear.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0054_mask_clear || {
    echo "not ok 1 - high-color mask clear"
    exit 0
}
echo "ok 1 - high-color mask clear"
exit 0
