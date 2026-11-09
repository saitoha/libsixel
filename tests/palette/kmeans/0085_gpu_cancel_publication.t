#!/bin/sh
# Verify gpu cancel publication.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}
echo "1..1"
set -v
run_status=0
SIXEL_GPU_POLICY=force SIXEL_PALETTE_KMEANS_BACKEND=gpu \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gpu-kmeans/0085_gpu_cancel_publication" || run_status=$?
test "$run_status" -ne 77 || {
    echo "ok 1 # SKIP Metal device is unavailable"
    exit 0
}
test "$run_status" -eq 0 || {
    echo "not ok 1 - gpu cancel publication"
    exit 0
}
echo "ok 1 - gpu cancel publication"
exit 0
