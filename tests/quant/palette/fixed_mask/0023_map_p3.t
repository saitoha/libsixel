#!/bin/sh
# Verify fixed palette mask: map p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0023_map_p3 \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" || {
    echo "not ok 1 - fixed palette mask map p3"
    exit 0
}

echo "ok 1 - fixed palette mask map p3"
exit 0
