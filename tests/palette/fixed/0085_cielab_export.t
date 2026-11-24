#!/bin/sh
# Verify native monochrome: cielab export.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0027_cielab_export || {
    printf '\n%s\n' "not ok 1 - mono cielab export"
    exit 0
}

printf '\n%s\n' "ok 1 - mono cielab export"
exit 0
