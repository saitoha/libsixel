#!/bin/sh
# Verify fhedt r64 f1 d1 lso2 history deferred.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup fhedt \
    "dither/gpu-direct/0077_fhedt_r64_f1_d1_lso2_history_deferred" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt r64 f1 d1 lso2 history deferred"
    exit 0
}
echo "ok 1 - fhedt r64 f1 d1 lso2 history deferred"
exit 0
