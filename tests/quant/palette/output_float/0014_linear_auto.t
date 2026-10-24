#!/bin/sh
# Verify linear auto native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0014_linear_auto || {
    echo "not ok 1 - linear auto native output"
    exit 0
}

echo "ok 1 - linear auto native output"
exit 0
