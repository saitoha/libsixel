#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify delay cancellation.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0113_delay_cancellation"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - delay cancellation # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - delay cancellation"
    exit 0
}
echo "ok 1 - delay cancellation"
exit 0
