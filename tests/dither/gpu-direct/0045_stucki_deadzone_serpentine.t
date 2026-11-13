#!/bin/sh
# Verify stucki deadzone serpentine.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0045_stucki_deadzone_serpentine" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - stucki deadzone serpentine"
    exit 0
}
echo "ok 1 - stucki deadzone serpentine"
exit 0
