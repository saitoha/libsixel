#!/bin/sh
# Verify native Heckbert infinity.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0018_heckbert_infinity >/dev/null || {
    echo "not ok 1 - native Heckbert infinity"
    exit 0
}

echo "ok 1 - native Heckbert infinity"
exit 0
