#!/bin/sh
# A timed-out operation reports a timeout failure.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" status/0001_status_timeout || {
    echo "not ok 1 - a timed-out operation reports a timeout failure"
    exit 0
}
echo "ok 1 - a timed-out operation reports a timeout failure"
exit 0
