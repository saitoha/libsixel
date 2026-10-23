#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: fs feedback.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0013_fs_feedback || {
    echo "not ok 1 - animation fs feedback"
    exit 0
}

echo "ok 1 - animation fs feedback"
exit 0
