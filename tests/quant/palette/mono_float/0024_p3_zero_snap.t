#!/bin/sh
# Verify native monochrome: p3 zero snap.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0024_p3_zero_snap || {
    echo "not ok 1 - mono p3 zero snap"
    exit 0
}

echo "ok 1 - mono p3 zero snap"
exit 0
