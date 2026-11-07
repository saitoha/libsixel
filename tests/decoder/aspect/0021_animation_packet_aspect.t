#!/bin/sh
# Verify animation packet aspect.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0021_animation_packet_aspect" || {
    echo "not ok 1 - animation packet aspect"
    exit 0
}
echo "ok 1 - animation packet aspect"
exit 0
