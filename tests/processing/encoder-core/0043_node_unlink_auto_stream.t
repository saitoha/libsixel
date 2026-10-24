#!/bin/sh
# Preserve the auto wire stream across node removals and worker counts.
set -eux

echo "1..1"
set -v

# Fixed worker counts exercise the same paths on small CI machines.
SIXEL_ENCODER_THREADING_POLICY=greedy
export SIXEL_ENCODER_THREADING_POLICY

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0043_node_unlink_auto_stream" || {
    echo "not ok 1 - auto node unlink wire snapshots"
    exit 0
}

echo "ok 1 - auto node unlink wire snapshots"
exit 0
