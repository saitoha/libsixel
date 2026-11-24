#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: fs soft feedback.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0015_fs_soft_feedback || {
    printf '\n%s\n' "not ok 1 - animation fs soft feedback"
    exit 0
}

printf '\n%s\n' "ok 1 - animation fs soft feedback"
exit 0
