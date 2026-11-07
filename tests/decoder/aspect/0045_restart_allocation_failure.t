#!/bin/sh
# Verify restart allocation failure.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0045_restart_allocation_failure" || {
    echo "not ok 1 - restart allocation failure"
    exit 0
}
echo "ok 1 - restart allocation failure"
exit 0
