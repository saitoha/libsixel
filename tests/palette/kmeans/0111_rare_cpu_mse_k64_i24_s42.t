#!/bin/sh
# Verify rare cpu mse k64 i24 s42.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/kmeans/0111_rare_cpu_mse_k64_i24_s42" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - rare cpu mse k64 i24 s42"
    exit 0
}
echo "ok 1 - rare cpu mse k64 i24 s42"
exit 0
