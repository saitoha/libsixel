#!/bin/sh
# Verify threshold limit instance isolation.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/palette/0129_threshold_limit_instance_isolation" || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - threshold limit instance isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - threshold limit instance isolation"
exit 0
