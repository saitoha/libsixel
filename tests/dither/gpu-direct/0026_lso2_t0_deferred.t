#!/bin/sh
# Verify lso2 t0 deferred.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "dither/gpu-direct/0026_lso2_t0_deferred" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - lso2 t0 deferred"
    exit 0
}
printf '\n%s\n' "ok 1 - lso2 t0 deferred"
exit 0
