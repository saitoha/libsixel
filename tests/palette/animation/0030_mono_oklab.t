#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mono oklab.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0009_mono_oklab || {
    printf '\n%s\n' "not ok 1 - fixed animation mono oklab"
    exit 0
}

printf '\n%s\n' "ok 1 - fixed animation mono oklab"
exit 0
