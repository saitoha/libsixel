#!/bin/sh
# Policy: docs/threading/decoder.md
# Verify a local column allocation failure leaves fallback buffers untouched.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0101_parallel_columns_allocation_failure" || {
    echo "not ok 1 - 0101_parallel_columns_allocation_failure"
    exit 0
}

echo "ok 1 - 0101_parallel_columns_allocation_failure"
exit 0
