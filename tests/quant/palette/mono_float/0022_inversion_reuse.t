#!/bin/sh
# Verify native monochrome: inversion reuse.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0022_inversion_reuse || {
    echo "not ok 1 - mono inversion reuse"
    exit 0
}

echo "ok 1 - mono inversion reuse"
exit 0
