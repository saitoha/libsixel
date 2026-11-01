#!/bin/sh
# Verify native Heckbert subbyte.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0009_heckbert_subbyte >/dev/null || {
    echo "not ok 1 - native Heckbert subbyte"
    exit 0
}

echo "ok 1 - native Heckbert subbyte"
exit 0
