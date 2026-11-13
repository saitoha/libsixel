#!/bin/sh
# Verify threshold limit reject 256.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/palette/0128_threshold_limit_reject_256" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - threshold limit reject 256"
    exit 0
}
echo "ok 1 - threshold limit reject 256"
exit 0
