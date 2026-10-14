#!/bin/sh
# Verify weighted lloyd in GPU k-means.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

echo "1..1"
set -v

run_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gpu-kmeans/0001_weighted_lloyd" || run_status=$?
test "$run_status" -ne 77 || {
    echo "ok 1 # SKIP Metal device is unavailable"
    exit 0
}
test "$run_status" -eq 0 || {
    echo "not ok 1 - weighted lloyd"
    exit 0
}
echo "ok 1 - weighted lloyd"
exit 0
