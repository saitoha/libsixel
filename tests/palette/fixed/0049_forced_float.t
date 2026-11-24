#!/bin/sh
# Verify fixed palette mask: forced float.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0021_forced_float || {
    printf '\n%s\n' "not ok 1 - fixed palette mask forced float"
    exit 0
}

printf '\n%s\n' "ok 1 - fixed palette mask forced float"
exit 0
