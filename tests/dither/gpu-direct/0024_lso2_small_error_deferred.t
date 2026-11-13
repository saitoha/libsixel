#!/bin/sh
# Verify lso2 small error deferred.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0024_lso2_small_error_deferred" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - lso2 small error deferred"
    exit 0
}
echo "ok 1 - lso2 small error deferred"
exit 0
