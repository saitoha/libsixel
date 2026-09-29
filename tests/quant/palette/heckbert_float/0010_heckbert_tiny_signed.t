#!/bin/sh
# Verify native Heckbert tiny signed.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0010_heckbert_tiny_signed >/dev/null || {
    echo "not ok 1 - native Heckbert tiny signed"
    exit 0
}

echo "ok 1 - native Heckbert tiny signed"
exit 0
