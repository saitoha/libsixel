#!/bin/sh
# Verify gpu nonfinite fallback for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0165_gpu_nonfinite_fallback"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu nonfinite fallback # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu nonfinite fallback"
    exit 0
}
echo "ok 1 - gpu nonfinite fallback"
exit 0
