#!/bin/sh
# Verify smptec medoids native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0016_smptec_medoids || {
    echo "not ok 1 - smptec medoids native output"
    exit 0
}

echo "ok 1 - smptec medoids native output"
exit 0
