#!/bin/sh
# Verify native Heckbert ward linear.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0019_heckbert_ward_linear >/dev/null || {
    echo "not ok 1 - native Heckbert ward linear"
    exit 0
}

echo "ok 1 - native Heckbert ward linear"
exit 0
