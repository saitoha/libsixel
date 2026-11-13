#!/bin/sh
# Verify snake cpu mse k64 i12 s7.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0113_snake_cpu_mse_k64_i12_s7" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - snake cpu mse k64 i12 s7"
    exit 0
}
echo "ok 1 - snake cpu mse k64 i12 s7"
exit 0
