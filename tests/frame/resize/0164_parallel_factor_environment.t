#!/bin/sh
# Run the scale parallel factor environment unit test.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0001_parallel_factor_environment" || {
    echo "not ok 1 - parallel factor missed band planning"
    exit 0
}

echo "ok 1 - parallel factor reaches band planning"
exit 0
