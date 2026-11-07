#!/bin/sh
# Verify lazy detection boundaries.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0042_lazy_boundaries" || {
    echo "not ok 1 - lazy detection boundaries"
    exit 0
}
echo "ok 1 - lazy detection boundaries"
exit 0
