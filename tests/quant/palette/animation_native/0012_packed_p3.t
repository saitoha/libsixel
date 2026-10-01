#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: packed p3.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0012_packed_p3 || {
    echo "not ok 1 - animation packed p3"
    exit 0
}

echo "ok 1 - animation packed p3"
exit 0
