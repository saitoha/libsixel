#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: gamma values.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0001_gamma_values || {
    printf '\n%s\n' "not ok 1 - animation gamma values"
    exit 0
}

printf '\n%s\n' "ok 1 - animation gamma values"
exit 0
