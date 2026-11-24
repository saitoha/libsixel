#!/bin/sh
# Verify spatial blue gpu fused keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0093_spatial_blue_gpu_fused_keep || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial blue gpu fused keep"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial blue gpu fused keep"
exit 0
