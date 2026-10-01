#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: map precision request.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0024_map_precision_request \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.gpl" || {
    echo "not ok 1 - fixed animation map precision request"
    exit 0
}

echo "ok 1 - fixed animation map precision request"
exit 0
