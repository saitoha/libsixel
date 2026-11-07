#!/bin/sh
# Verify ormode rows.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0014_ormode_rows" || {
    echo "not ok 1 - ormode rows"
    exit 0
}
echo "ok 1 - ormode rows"
exit 0
