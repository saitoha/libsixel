#!/bin/sh
# Verify restart fused fast4.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0044_restart_fused_fast4" || {
    printf '\n%s\n' "not ok 1 - restart fused fast4"
    exit 0
}
printf '\n%s\n' "ok 1 - restart fused fast4"
exit 0
