#!/bin/sh
# Verify mid band overlap.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0039_mid_band_overlap" || {
    printf '\n%s\n' "not ok 1 - mid band overlap"
    exit 0
}
printf '\n%s\n' "ok 1 - mid band overlap"
exit 0
