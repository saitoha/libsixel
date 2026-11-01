#!/bin/sh
# Verify operation priority.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0008_operation_priority"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - operation priority # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - operation priority"
    exit 0
}
echo "ok 1 - operation priority"
exit 0
