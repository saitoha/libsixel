#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: packed byte fast.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0011_packed_byte_fast || {
    echo "not ok 1 - animation packed byte fast"
    exit 0
}

echo "ok 1 - animation packed byte fast"
exit 0
