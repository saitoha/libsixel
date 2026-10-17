#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify worker memo exact for the common float transaction.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/parallel/0012_worker_memo_exact" || {
    echo "not ok 1 - worker memo exact"
    exit 0
}
echo "ok 1 - worker memo exact"
exit 0
