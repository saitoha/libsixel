#!/bin/sh
# Policy: docs/threading/decoder.md
# Resolve absolute rows before trusted-raster clipping and flag updates.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0092_parallel_prefix_clipping" || {
    echo "not ok 1 - parallel_prefix_clipping"
    exit 0
}

echo "ok 1 - parallel_prefix_clipping"
exit 0
