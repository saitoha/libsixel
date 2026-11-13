#!/bin/sh
# Verify rgb cell 6bit lso2 history run.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup 6bit \
    "dither/gpu-direct/0100_rgb_cell_6bit_lso2_history_run" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - rgb cell 6bit lso2 history run"
    exit 0
}
echo "ok 1 - rgb cell 6bit lso2 history run"
exit 0
