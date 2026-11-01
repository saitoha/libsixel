#!/bin/sh
# Verify gpu request validation for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0158_gpu_request_validation"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu request validation # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu request validation"
    exit 0
}
echo "ok 1 - gpu request validation"
exit 0
