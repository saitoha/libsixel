#!/bin/sh
# Verify fixed palette mask: wide image map.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0026_wide_image_map \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" || {
    echo "not ok 1 - fixed palette mask wide image map"
    exit 0
}

echo "ok 1 - fixed palette mask wide image map"
exit 0
