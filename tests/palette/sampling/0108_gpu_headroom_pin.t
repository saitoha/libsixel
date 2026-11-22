#!/bin/sh
# Verify both profiling pins and the explicit CPU policy.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
_SIXEL_TEST_GPU_HEADROOM=gpu \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling/0108_gpu_headroom_pin gpu || {
    echo 'not ok 1 - GPU headroom pin'
    exit 0
}
_SIXEL_TEST_GPU_HEADROOM=cpu \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/sampling/0108_gpu_headroom_pin cpu || {
    echo 'not ok 1 - CPU headroom pin'
    exit 0
}
echo 'ok 1 - GPU headroom pin'
exit 0
