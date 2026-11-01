#!/bin/sh
# Preserve the fast wire stream across node removals and worker counts.
set -eux

echo "1..1"
set -v

# Fixed worker counts exercise the same paths on small CI machines.
SIXEL_ENCODER_THREADING_POLICY=greedy
export SIXEL_ENCODER_THREADING_POLICY

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0042_node_unlink_fast_stream" || {
    echo "not ok 1 - fast node unlink wire snapshots"
    exit 0
}

echo "ok 1 - fast node unlink wire snapshots"
exit 0
