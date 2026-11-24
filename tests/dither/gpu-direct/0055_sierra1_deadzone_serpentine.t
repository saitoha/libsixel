#!/bin/sh
# Verify sierra1 deadzone serpentine.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0055_sierra1_deadzone_serpentine" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - sierra1 deadzone serpentine"
    exit 0
}
printf '\n%s\n' "ok 1 - sierra1 deadzone serpentine"
exit 0
