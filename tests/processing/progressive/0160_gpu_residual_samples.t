#!/bin/sh
# Verify gpu residual samples for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0160_gpu_residual_samples"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu residual samples # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu residual samples"
    exit 0
}
echo "ok 1 - gpu residual samples"
exit 0
