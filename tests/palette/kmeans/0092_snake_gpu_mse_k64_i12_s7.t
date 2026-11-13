#!/bin/sh
# Verify snake gpu mse k64 i12 s7.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0092_snake_gpu_mse_k64_i12_s7" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - snake gpu mse k64 i12 s7"
    exit 0
}
echo "ok 1 - snake gpu mse k64 i12 s7"
exit 0
