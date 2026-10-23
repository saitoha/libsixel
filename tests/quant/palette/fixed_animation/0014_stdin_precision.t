#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: stdin precision.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0014_stdin_precision <<'PALETTE' || {
GIMP Palette
88 88 88
94 94 94
88 88 88
255 0 0
PALETTE
    echo "not ok 1 - fixed animation stdin precision"
    exit 0
}

echo "ok 1 - fixed animation stdin precision"
exit 0
