#!/bin/sh
# Verify output reuse native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0027_output_reuse || {
    echo "not ok 1 - output reuse native output"
    exit 0
}

echo "ok 1 - output reuse native output"
exit 0
