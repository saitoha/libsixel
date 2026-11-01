#!/bin/sh
# Verify smptec linear native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0007_smptec_linear || {
    echo "not ok 1 - smptec linear native output"
    exit 0
}

echo "ok 1 - smptec linear native output"
exit 0
