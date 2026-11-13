#!/bin/sh
# Verify lso2 t0 run.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0025_lso2_t0_run" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - lso2 t0 run"
    exit 0
}
echo "ok 1 - lso2 t0 run"
exit 0
