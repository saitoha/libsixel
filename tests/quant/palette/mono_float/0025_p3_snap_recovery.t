#!/bin/sh
# Verify native monochrome: p3 snap recovery.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0025_p3_snap_recovery || {
    echo "not ok 1 - mono p3 snap recovery"
    exit 0
}

echo "ok 1 - mono p3 snap recovery"
exit 0
