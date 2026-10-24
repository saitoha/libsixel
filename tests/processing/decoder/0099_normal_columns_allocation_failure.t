#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal column allocation cleanup.
set -eux
echo "1..1"
set -v

SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0099_normal_columns_allocation_failure" || {
    echo "not ok 1 - normal_columns_allocation_failure"
    exit 0
}
echo "ok 1 - normal_columns_allocation_failure"
exit 0
