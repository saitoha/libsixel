#!/bin/sh
# Policy: docs/threading/decoder.md
# Validate a large paint extent before clipping to a one-row canvas.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0093_parallel_large_clipped_scan" || {
    echo "not ok 1 - parallel_large_clipped_scan"
    exit 0
}

echo "ok 1 - parallel_large_clipped_scan"
exit 0
