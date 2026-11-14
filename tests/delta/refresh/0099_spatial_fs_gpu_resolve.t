#!/bin/sh
# Check the spatial luma gate in the Floyd–Steinberg GPU resolve pass.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0099_spatial_fs_gpu_resolve || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - FS GPU resolve uses the spatial luma gate"
    exit 0
}
echo "ok 1 - FS GPU resolve uses the spatial luma gate"
exit 0
