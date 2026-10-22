#!/bin/sh
# Verify 8bit applies a float source as bytes with linear output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0023_linear_8bit_preference || {
    echo "not ok 1 - linear 8bit preference native output"
    exit 0
}

echo "ok 1 - linear 8bit preference native output"
exit 0
