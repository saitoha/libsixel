#!/bin/sh
# Verify clear map sparse parallel.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0060_clear_map_sparse_parallel" || {
    echo "not ok 1 - clear map sparse parallel"
    exit 0
}

echo "ok 1 - clear map sparse parallel"
exit 0
