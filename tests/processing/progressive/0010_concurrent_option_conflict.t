#!/bin/sh
# Verify concurrent option conflict.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0010_concurrent_option_conflict"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - concurrent option conflict # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - concurrent option conflict"
    exit 0
}
echo "ok 1 - concurrent option conflict"
exit 0
