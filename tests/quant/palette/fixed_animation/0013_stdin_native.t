#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: stdin native.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0013_stdin_native <<'PALETTE' || {
GIMP Palette
88 88 88
94 94 94
88 88 88
255 0 0
PALETTE
    echo "not ok 1 - fixed animation stdin native"
    exit 0
}

echo "ok 1 - fixed animation stdin native"
exit 0
