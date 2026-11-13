#!/bin/sh
# Verify sierra2 deadzone raster.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0059_sierra2_deadzone_raster" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - sierra2 deadzone raster"
    exit 0
}
echo "ok 1 - sierra2 deadzone raster"
exit 0
