#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: fs soft feedback.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0015_fs_soft_feedback || {
    echo "not ok 1 - animation fs soft feedback"
    exit 0
}

echo "ok 1 - animation fs soft feedback"
exit 0
