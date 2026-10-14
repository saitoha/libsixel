#!/bin/sh
# Verify failed background allocation.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0012_failed_background_allocation"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - failed background allocation # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - failed background allocation"
    exit 0
}
echo "ok 1 - failed background allocation"
exit 0
