#!/bin/sh
# Verify gradient gpu mse k16 i4 s1.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0069_gradient_gpu_mse_k16_i4_s1" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - gradient gpu mse k16 i4 s1"
    exit 0
}
printf '\n%s\n' "ok 1 - gradient gpu mse k16 i4 s1"
exit 0
