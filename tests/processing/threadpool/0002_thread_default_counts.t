#!/bin/sh
# Run the thread default count unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0002_thread_default_counts" || {
    echo "not ok 1 - 0002_thread_default_counts"
    exit 0
}

echo "ok 1 - 0002_thread_default_counts"
exit 0
