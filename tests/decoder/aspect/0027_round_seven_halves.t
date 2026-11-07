#!/bin/sh
# Verify round seven halves.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0027_round_seven_halves" || {
    echo "not ok 1 - round seven halves"
    exit 0
}
echo "ok 1 - round seven halves"
exit 0
