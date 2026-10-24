#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify frame bits for the common float transaction.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v

# Fixed worker counts exercise the same paths on small CI machines.
SIXEL_ENCODER_THREADING_POLICY=greedy
export SIXEL_ENCODER_THREADING_POLICY
SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/parallel/0001_frame_bits" || {
    echo "not ok 1 - frame bits"
    exit 0
}
echo "ok 1 - frame bits"
exit 0
