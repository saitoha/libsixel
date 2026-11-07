#!/bin/sh
# Verify high color rgba.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0022_high_color_rgba" || {
    echo "not ok 1 - high color rgba"
    exit 0
}
echo "ok 1 - high color rgba"
exit 0
