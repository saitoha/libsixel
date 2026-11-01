#!/bin/sh
# Verify source cancellation preserves INTERRUPTED and publishes no chunk.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" chunk/0002_chunk_cancel_callback \
    "${TOP_SRCDIR}/images/snake.png" || {
    echo "not ok 1 - cancelled source returns INTERRUPTED without a chunk"
    exit 0
}
echo "ok 1 - cancelled source returns INTERRUPTED without a chunk"
exit 0
