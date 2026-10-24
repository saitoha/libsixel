#!/bin/sh
# Verify native monochrome: empty mask.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0030_empty_mask || {
    echo "not ok 1 - mono empty mask"
    exit 0
}

echo "ok 1 - mono empty mask"
exit 0
