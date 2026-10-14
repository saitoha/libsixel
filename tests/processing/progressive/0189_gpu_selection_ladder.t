#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Keep frozen mask thresholds and protected candidates on the GPU path.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0189_gpu_selection_ladder"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu selection ladder # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu selection ladder"
    exit 0
}
echo "ok 1 - gpu selection ladder"
exit 0
