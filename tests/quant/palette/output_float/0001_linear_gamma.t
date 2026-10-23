#!/bin/sh
# Verify linear gamma native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0001_linear_gamma || {
    echo "not ok 1 - linear gamma native output"
    exit 0
}

echo "ok 1 - linear gamma native output"
exit 0
