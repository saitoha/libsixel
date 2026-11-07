#!/bin/sh
# Verify mid band change.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0030_mid_band_change" || {
    echo "not ok 1 - mid band change"
    exit 0
}
echo "ok 1 - mid band change"
exit 0
