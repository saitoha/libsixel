#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: gamma values.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0001_gamma_values || {
    echo "not ok 1 - animation gamma values"
    exit 0
}

echo "ok 1 - animation gamma values"
exit 0
