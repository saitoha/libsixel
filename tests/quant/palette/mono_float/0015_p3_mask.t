#!/bin/sh
# Verify native monochrome: p3 mask.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0015_p3_mask || {
    echo "not ok 1 - mono p3 mask"
    exit 0
}

echo "ok 1 - mono p3 mask"
exit 0
