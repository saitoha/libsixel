#!/bin/sh
# Verify spatial blue gpu fused paint.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0094_spatial_blue_gpu_fused_paint || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - spatial blue gpu fused paint"
    exit 0
}
echo "ok 1 - spatial blue gpu fused paint"
exit 0
