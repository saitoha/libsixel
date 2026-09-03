#!/bin/sh
# Run the scale parallel threshold environment unit test.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0002_parallel_min_bytes_environment" || {
    echo "not ok 1 - scale threshold missed parallel dispatch"
    exit 0
}

echo "ok 1 - scale threshold reaches parallel dispatch"
exit 0
