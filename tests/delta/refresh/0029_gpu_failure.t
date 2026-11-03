#!/bin/sh
# Verify GPU partial rectangle gpu failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette delta/refresh/0029_gpu_failure || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU partial rectangle gpu failure"
    exit 0
}
echo "ok 1 - GPU partial rectangle gpu failure"
exit 0
