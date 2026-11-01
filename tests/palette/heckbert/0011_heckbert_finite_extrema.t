#!/bin/sh
# Verify native Heckbert finite extrema.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0011_heckbert_finite_extrema >/dev/null || {
    echo "not ok 1 - native Heckbert finite extrema"
    exit 0
}

echo "ok 1 - native Heckbert finite extrema"
exit 0
