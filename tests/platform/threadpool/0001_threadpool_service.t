#!/bin/sh
# Run the threadpool service component unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0001_threadpool_service" || {
    echo "not ok 1 - 0001_threadpool_service"
    exit 0
}

echo "ok 1 - 0001_threadpool_service"
exit 0
