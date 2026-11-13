#!/bin/sh
# Verify rgb cell 5bit fs deadzone.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup 5bit \
    "dither/gpu-direct/0016_rgb_cell_5bit_fs_deadzone" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - rgb cell 5bit fs deadzone"
    exit 0
}
echo "ok 1 - rgb cell 5bit fs deadzone"
exit 0
