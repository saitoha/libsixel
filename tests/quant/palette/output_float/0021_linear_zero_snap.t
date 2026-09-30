#!/bin/sh
# Verify linear zero snap native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0021_linear_zero_snap || {
    echo "not ok 1 - linear zero snap native output"
    exit 0
}

echo "ok 1 - linear zero snap native output"
exit 0
