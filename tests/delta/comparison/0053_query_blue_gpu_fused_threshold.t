#!/bin/sh
# Verify query blue gpu fused threshold.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "delta/comparison/0053_query_blue_gpu_fused_threshold" >/dev/null || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - query blue gpu fused threshold"
    exit 0
}

echo "ok 1 - query blue gpu fused threshold"
exit 0
