#!/bin/sh
# Verify native alpha: heckbert.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0013_heckbert || {
    printf '\n%s\n' "not ok 1 - native alpha heckbert"
    exit 0
}

printf '\n%s\n' "ok 1 - native alpha heckbert"
exit 0
