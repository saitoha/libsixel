#!/bin/sh
# Verify unit then double bands.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0028_unit_then_double_bands" || {
    echo "not ok 1 - unit then double bands"
    exit 0
}
echo "ok 1 - unit then double bands"
exit 0
