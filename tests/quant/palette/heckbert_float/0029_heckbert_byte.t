#!/bin/sh
# Verify native Heckbert byte.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0029_heckbert_byte >/dev/null || {
    echo "not ok 1 - native Heckbert byte"
    exit 0
}

echo "ok 1 - native Heckbert byte"
exit 0
