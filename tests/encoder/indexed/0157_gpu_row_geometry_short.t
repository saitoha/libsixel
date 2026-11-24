#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" component/gpu_row_geometry_short || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - GPU chunk geometry for short raster."
    exit 0
}
printf '\n%s\n' "ok 1 - GPU chunk geometry for short raster."
exit 0
