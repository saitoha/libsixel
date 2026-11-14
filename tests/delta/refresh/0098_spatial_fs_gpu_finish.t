#!/bin/sh
# Check the spatial luma gate in the Floyd–Steinberg GPU finish pass.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0098_spatial_fs_gpu_finish || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required runtime is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - FS GPU finish uses the spatial luma gate"
    exit 0
}
echo "ok 1 - FS GPU finish uses the spatial luma gate"
exit 0
