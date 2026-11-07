#!/bin/sh
# Verify changing wide ormode.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0041_changing_wide_ormode" || {
    echo "not ok 1 - changing wide ormode"
    exit 0
}
echo "ok 1 - changing wide ormode"
exit 0
