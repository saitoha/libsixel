#!/bin/sh
# none GPU RGB preserves legacy stream bytes.
# Policy: docs/encoder/execution-map/README.md
set -eux
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || status=$?
test "$status" -ne 77 || {
    echo '1..0 # SKIP Metal device is unavailable'
    exit 0
}
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0034_none_gpu_rgb_bytes" || {
    echo 'not ok 1 - 0034_none_gpu_rgb_bytes'
    exit 0
}
echo 'ok 1 - 0034_none_gpu_rgb_bytes'
exit 0
