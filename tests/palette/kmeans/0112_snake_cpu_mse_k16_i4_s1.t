#!/bin/sh
# Verify snake cpu mse k16 i4 s1.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0112_snake_cpu_mse_k16_i4_s1" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - snake cpu mse k16 i4 s1"
    exit 0
}
echo "ok 1 - snake cpu mse k16 i4 s1"
exit 0
