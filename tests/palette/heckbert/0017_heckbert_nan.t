#!/bin/sh
# Verify native Heckbert nan.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0017_heckbert_nan >/dev/null || {
    echo "not ok 1 - native Heckbert nan"
    exit 0
}

echo "ok 1 - native Heckbert nan"
exit 0
