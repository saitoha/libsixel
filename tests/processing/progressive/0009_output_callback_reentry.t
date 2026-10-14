#!/bin/sh
# Verify output callback reentry.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0009_output_callback_reentry"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - output callback reentry # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - output callback reentry"
    exit 0
}
echo "ok 1 - output callback reentry"
exit 0
