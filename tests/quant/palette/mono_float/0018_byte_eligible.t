#!/bin/sh
# Verify native monochrome: byte eligible.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0018_byte_eligible || {
    echo "not ok 1 - mono byte eligible"
    exit 0
}

echo "ok 1 - mono byte eligible"
exit 0
