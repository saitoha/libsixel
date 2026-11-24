#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Encoder hard AUTO preserves 32 native colors and RGB101 output.
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0009_encoder_hard_wire || {
    printf '\n%s\n' "not ok 1 - Encoder hard AUTO preserves 32 native colors and RGB101 output"
    exit 0
}
printf '\n%s\n' "ok 1 - Encoder hard AUTO preserves 32 native colors and RGB101 output"
exit 0
