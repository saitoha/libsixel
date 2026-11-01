#!/bin/sh
# Verify gpu allocation failure for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0167_gpu_allocation_failure"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu allocation failure # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu allocation failure"
    exit 0
}
echo "ok 1 - gpu allocation failure"
exit 0
