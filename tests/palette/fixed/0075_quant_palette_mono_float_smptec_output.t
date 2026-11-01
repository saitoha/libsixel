#!/bin/sh
# Verify native monochrome: smptec output.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0017_smptec_output || {
    echo "not ok 1 - mono smptec output"
    exit 0
}

echo "ok 1 - mono smptec output"
exit 0
