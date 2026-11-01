#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: image p3.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0019_image_p3 \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" || {
    echo "not ok 1 - fixed animation image p3"
    exit 0
}

echo "ok 1 - fixed animation image p3"
exit 0
