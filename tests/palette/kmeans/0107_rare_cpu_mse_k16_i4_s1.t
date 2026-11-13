#!/bin/sh
# Verify rare cpu mse k16 i4 s1.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0107_rare_cpu_mse_k16_i4_s1" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - rare cpu mse k16 i4 s1"
    exit 0
}
echo "ok 1 - rare cpu mse k16 i4 s1"
exit 0
