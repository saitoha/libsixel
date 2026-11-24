#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0067_loader_wait_for_condition" || {
    printf '\n%s\n' "not ok 1 - loader bounded wait helper"
    exit 0
}

printf '\n%s\n' "ok 1 - loader bounded wait helper"
exit 0
