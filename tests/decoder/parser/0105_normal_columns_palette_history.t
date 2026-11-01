#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns palette history.
set -eux
echo "1..1"
set -v

SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0095_normal_columns_palette_history" || {
    echo "not ok 1 - normal_columns_palette_history"
    exit 0
}
echo "ok 1 - normal_columns_palette_history"
exit 0
