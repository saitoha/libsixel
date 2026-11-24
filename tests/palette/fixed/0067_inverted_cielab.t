#!/bin/sh
# Verify native monochrome: inverted cielab.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0009_inverted_cielab || {
    printf '\n%s\n' "not ok 1 - mono inverted cielab"
    exit 0
}

printf '\n%s\n' "ok 1 - mono inverted cielab"
exit 0
