#!/bin/sh
# Verify query a gpu fused early bypass.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "delta/comparison/0029_query_a_gpu_fused_early_bypass" >/dev/null || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - query a gpu fused early bypass"
    exit 0
}

echo "ok 1 - query a gpu fused early bypass"
exit 0
