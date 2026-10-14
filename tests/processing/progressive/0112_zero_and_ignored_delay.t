#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify zero and ignored delay.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0112_zero_and_ignored_delay"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - zero and ignored delay # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - zero and ignored delay"
    exit 0
}
echo "ok 1 - zero and ignored delay"
exit 0
