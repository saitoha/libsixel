#!/bin/sh
# Verify native Heckbert center.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0001_heckbert_center >/dev/null || {
    echo "not ok 1 - native Heckbert center"
    exit 0
}

echo "ok 1 - native Heckbert center"
exit 0
