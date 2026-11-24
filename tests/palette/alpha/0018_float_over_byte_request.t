#!/bin/sh
# Verify native alpha: 8bit applies float input through bytes.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0018_float_over_byte_request || {
    printf '\n%s\n' "not ok 1 - native alpha float over byte request"
    exit 0
}

printf '\n%s\n' "ok 1 - native alpha float over byte request"
exit 0
