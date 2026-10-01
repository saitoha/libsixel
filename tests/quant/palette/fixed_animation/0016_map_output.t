#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: map output.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0016_map_output \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.gpl" || {
    echo "not ok 1 - fixed animation map output"
    exit 0
}

echo "ok 1 - fixed animation map output"
exit 0
