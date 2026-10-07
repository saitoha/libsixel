#!/bin/sh
# Interpreted zero alpha and side holes are both omitted.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0075_packed_mask_union || {
    echo "not ok 1 - high-color packed mask union"
    exit 0
}
echo "ok 1 - high-color packed mask union"
exit 0
