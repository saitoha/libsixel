#!/bin/sh
# Run the threadpool service component unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0001_threadpool_service" || {
    printf '\n%s\n' "not ok 1 - 0001_threadpool_service"
    exit 0
}

printf '\n%s\n' "ok 1 - 0001_threadpool_service"
exit 0
