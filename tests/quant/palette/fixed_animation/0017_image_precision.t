#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: image precision.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0017_image_precision \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" || {
    echo "not ok 1 - fixed animation image precision"
    exit 0
}

echo "ok 1 - fixed animation image precision"
exit 0
