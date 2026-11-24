#!/bin/sh
# A timed-out operation reports a timeout failure.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" status/0001_status_timeout || {
    printf '\n%s\n' "not ok 1 - a timed-out operation reports a timeout failure"
    exit 0
}
printf '\n%s\n' "ok 1 - a timed-out operation reports a timeout failure"
exit 0
