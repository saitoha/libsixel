#!/bin/sh
# Verify gpu policy admission for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0159_gpu_policy_admission"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu policy admission # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu policy admission"
    exit 0
}
echo "ok 1 - gpu policy admission"
exit 0
