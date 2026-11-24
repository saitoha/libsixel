#!/bin/sh
# Verify extended failure atomic.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/0012_extended_failure_atomic" || {
    printf '\n%s\n' "not ok 1 - extended failure atomic"
    exit 0
}

printf '\n%s\n' "ok 1 - extended failure atomic"
exit 0
