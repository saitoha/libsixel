#!/bin/sh
# Verify native monochrome: precision reuse.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0020_precision_reuse || {
    printf '\n%s\n' "not ok 1 - mono precision reuse"
    exit 0
}

printf '\n%s\n' "ok 1 - mono precision reuse"
exit 0
