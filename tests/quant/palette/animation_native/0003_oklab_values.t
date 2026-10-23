#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: oklab values.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0003_oklab_values || {
    echo "not ok 1 - animation oklab values"
    exit 0
}

echo "ok 1 - animation oklab values"
exit 0
