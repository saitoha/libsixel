#!/bin/sh
# Verify rgb cell 6bit fs deadzone.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup 6bit \
    "dither/gpu-direct/0097_rgb_cell_6bit_fs_deadzone" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - rgb cell 6bit fs deadzone"
    exit 0
}
printf '\n%s\n' "ok 1 - rgb cell 6bit fs deadzone"
exit 0
