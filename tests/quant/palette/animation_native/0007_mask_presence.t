#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: mask presence.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0007_mask_presence || {
    echo "not ok 1 - animation mask presence"
    exit 0
}

echo "ok 1 - animation mask presence"
exit 0
