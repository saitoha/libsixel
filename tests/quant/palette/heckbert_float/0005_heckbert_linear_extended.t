#!/bin/sh
# Verify native Heckbert linear extended.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0005_heckbert_linear_extended >/dev/null || {
    echo "not ok 1 - native Heckbert linear extended"
    exit 0
}

echo "ok 1 - native Heckbert linear extended"
exit 0
