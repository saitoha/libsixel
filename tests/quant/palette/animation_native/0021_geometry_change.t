#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: geometry change.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0021_geometry_change || {
    echo "not ok 1 - animation geometry change"
    exit 0
}

echo "ok 1 - animation geometry change"
exit 0
