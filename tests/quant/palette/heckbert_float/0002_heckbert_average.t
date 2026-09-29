#!/bin/sh
# Verify native Heckbert average.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0002_heckbert_average >/dev/null || {
    echo "not ok 1 - native Heckbert average"
    exit 0
}

echo "ok 1 - native Heckbert average"
exit 0
