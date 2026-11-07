#!/bin/sh
# Verify first late aspect.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0008_first_late_aspect" || {
    echo "not ok 1 - first late aspect"
    exit 0
}
echo "ok 1 - first late aspect"
exit 0
