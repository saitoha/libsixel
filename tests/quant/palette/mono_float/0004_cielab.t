#!/bin/sh
# Verify native monochrome: cielab.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0004_cielab || {
    echo "not ok 1 - mono cielab"
    exit 0
}

echo "ok 1 - mono cielab"
exit 0
