#!/bin/sh
# Verify partial row group.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0005_partial_row_group" || {
    echo "not ok 1 - partial row group"
    exit 0
}
echo "ok 1 - partial row group"
exit 0
