#!/bin/sh
# Verify yinyang ties.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v
run_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gpu-kmeans/0050_yinyang_ties" || run_status=$?
test "$run_status" -ne 77 || {
    echo "ok 1 # SKIP GPU device is unavailable"
    exit 0
}
test "$run_status" -eq 0 || {
    echo "not ok 1 - yinyang ties"
    exit 0
}
echo "ok 1 - yinyang ties"
exit 0
